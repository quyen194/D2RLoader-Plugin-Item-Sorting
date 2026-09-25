#include "sort_engine.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace RemoteStashSorting {

namespace {

constexpr uint32_t kQualityMagic   = 4;
constexpr uint32_t kQualitySet     = 5;
constexpr uint32_t kQualityRare    = 6;
constexpr uint32_t kQualityUnique  = 7;
constexpr uint32_t kQualityCrafted = 8;

bool IsUniqueLike(const ItemFacts& it) { return it.quality == kQualityUnique || it.quality == kQualityCrafted; }
bool IsRare(const ItemFacts& it) { return it.quality == kQualityRare; }
bool IsMagic(const ItemFacts& it) { return it.quality == kQualityMagic; }
bool IsSetQuality(const ItemFacts& it) { return it.quality == kQualitySet; }

// Crafted (q=8) sorts ahead of unique (q=7) inside each group -- mirrors
// _ssQualityRank.
int64_t QualityRank(const ItemFacts& it) { return it.quality == kQualityCrafted ? 0 : 1; }
int64_t IlvlDesc(const ItemFacts& it) { return -static_cast<int64_t>(it.itemLevel); }

// Coarser key that runs ahead of the gear-group index: armor first, then
// one-handed weapons, then two-handed. Mirrors _ssHandTier. GearSlot enum
// values Helm..Shield are declared first, matching SS_GEAR_GROUPS' armor
// block, so `slot <= Shield` identifies armor.
int64_t HandTier(const ItemFacts& it) {
	if (it.gearSlot == GearSlot::None) return 3;
	if (it.gearSlot >= GearSlot::Helm && it.gearSlot <= GearSlot::Shield) return 0;
	if (it.gearSlot == GearSlot::Arrows || it.gearSlot == GearSlot::Bolts) return 2;
	return it.isTwoHanded ? 2 : 1;
}

// Bucket index used as the primary gear sort key. GearSlot's declaration
// order matches SS_GEAR_GROUPS exactly (Helm, Torso, Belt, Gloves, Boots,
// Shield, Axe, Sword, Knife, ClubMace, Scepter, Wand, Staff, Orb, Spear,
// Polearm, Javelin, Bow, Arrows, Crossbow, Bolts, Claw, OtherWeapon), so the
// enum value doubles as the group index.
int64_t GearGroupIndex(const ItemFacts& it) { return static_cast<int64_t>(it.gearSlot); }

std::string TieBreakName(const ItemFacts& it) {
	return it.baseNameKey.empty() ? std::to_string(it.code) : it.baseNameKey;
}

// Generic 5-int + 1-string comparison key. Which field holds what varies by
// category (see BuildStreams) -- streams only ever compare items against
// other items matched by that same stream, so an unused slot just stays 0
// for everyone and drops out of the comparison.
struct SortKey {
	int64_t     k0 = 0, k1 = 0, k2 = 0, k3 = 0, k4 = 0;
	std::string text;
};

bool KeyLess(const SortKey& a, const SortKey& b) {
	if (a.k0 != b.k0) return a.k0 < b.k0;
	if (a.k1 != b.k1) return a.k1 < b.k1;
	if (a.k2 != b.k2) return a.k2 < b.k2;
	if (a.k3 != b.k3) return a.k3 < b.k3;
	if (a.k4 != b.k4) return a.k4 < b.k4;
	return a.text < b.text;
}

enum class FillDir { Row, ColLeft, ColRight };

struct Stream {
	std::function<bool(const ItemFacts&)>   match;
	FillDir                                 dir = FillDir::Row;
	std::function<SortKey(const ItemFacts&)> key;
};

struct Mode {
	std::string        id;
	bool                atomicSets = false;
	std::vector<Stream> streams;
};

// Mirrors SS_SORT_MODES / _ssRingAmuletModes. Ring fills from the left
// column, amulet from the right, so the two never interleave.
Stream RingAmuStream(bool isRing, std::function<bool(const ItemFacts&)> qualityMatch, bool ranked) {
	Stream s;
	s.dir = isRing ? FillDir::ColLeft : FillDir::ColRight;
	NonGearKind want = isRing ? NonGearKind::Ring : NonGearKind::Amulet;
	s.match = [qualityMatch, want](const ItemFacts& it) { return qualityMatch(it) && it.nonGearKind == want; };
	s.key = [ranked](const ItemFacts& it) {
		SortKey k;
		int i = 0;
		if (ranked) k.k0 = QualityRank(it);
		k.k1 = static_cast<int64_t>(it.sortLevel);
		k.k2 = IlvlDesc(it);
		k.text = TieBreakName(it);
		(void)i;
		return k;
	};
	return s;
}

const std::vector<Mode>& BuiltinModes() {
	static const std::vector<Mode> modes = [] {
		std::vector<Mode> m;

		m.push_back(Mode {
			.id         = "set",
			.atomicSets = true,
			.streams    = { Stream {
				   // Set-quality with an unresolved setGroupName (SetGroupForRow
				   // couldn't map qualityRecordId to a known set) is documented
				   // (item_classification.hpp, SetGroupForRow) to be treated
				   // "same as a completely unknown item" -- i.e. left alone.
				   // match() has to encode that itself: it is the one thing
				   // RunAutoSort's willBeMoved precomputation (see RunAutoSort)
				   // trusts to know "will this item actually get a move", and
				   // the atomic-set grouping loop below independently skips an
				   // empty setGroupName too. If match() only checked quality,
				   // willBeMoved would mark such an item as "about to vacate"
				   // while the grouping loop silently never placed it -- a real
				   // item still sitting exactly where some other item's plan
				   // now assumes is free. (Exactly the collision a live run
				   // surfaced: quality-5, empty setGroupName, still occupying
				   // its old cell while another item planned to land on it.)
				   .match = [](const ItemFacts& it) { return IsSetQuality(it) && !it.setGroupName.empty(); },
				   .dir   = FillDir::Row,
				   .key   = [](const ItemFacts&) { return SortKey {}; },
			   } },
		});

		m.push_back(Mode {
			.id      = "uniqueGear",
			.streams = { Stream {
				.match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.isGear; },
				.dir   = FillDir::Row,
				.key =
					[](const ItemFacts& it) {
					SortKey k;
					k.k0   = HandTier(it);
					k.k1   = GearGroupIndex(it);
					k.k2   = QualityRank(it);
					k.k3   = static_cast<int64_t>(it.sortLevel);
					k.k4   = IlvlDesc(it);
					k.text = TieBreakName(it);
					return k;
				},
			} },
		});

		m.push_back(Mode {
			.id      = "rareGear",
			.streams = { Stream {
				.match = [](const ItemFacts& it) { return IsRare(it) && it.isGear; },
				.dir   = FillDir::Row,
				.key =
					[](const ItemFacts& it) {
					SortKey k;
					k.k0   = HandTier(it);
					k.k1   = GearGroupIndex(it);
					k.k3   = static_cast<int64_t>(it.sortLevel);
					k.k4   = IlvlDesc(it);
					k.text = TieBreakName(it);
					return k;
				},
			} },
		});

		m.push_back(Mode {
			.id      = "uniqueCharm",
			.streams = { Stream {
				.match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.nonGearKind == NonGearKind::Charm; },
				.dir   = FillDir::Row,
				.key =
					[](const ItemFacts& it) {
					SortKey k;
					k.k0   = -static_cast<int64_t>(it.h);
					k.k1   = QualityRank(it);
					k.k2   = static_cast<int64_t>(it.sortLevel);
					k.k3   = IlvlDesc(it);
					k.text = TieBreakName(it);
					return k;
				},
			} },
		});

		m.push_back(Mode {
			.id      = "magicCharm",
			.streams = { Stream {
				.match = [](const ItemFacts& it) { return IsMagic(it) && it.nonGearKind == NonGearKind::Charm; },
				.dir   = FillDir::Row,
				.key =
					[](const ItemFacts& it) {
					SortKey k;
					k.k0   = -static_cast<int64_t>(it.h);
					k.k2   = static_cast<int64_t>(it.sortLevel);
					k.k3   = IlvlDesc(it);
					k.text = TieBreakName(it);
					return k;
				},
			} },
		});

		m.push_back(Mode {
			.id      = "uniqueJewel",
			.streams = { Stream {
				.match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.nonGearKind == NonGearKind::Jewel; },
				.dir   = FillDir::Row,
				.key =
					[](const ItemFacts& it) {
					SortKey k;
					k.k0   = QualityRank(it);
					k.k1   = static_cast<int64_t>(it.sortLevel);
					k.k2   = IlvlDesc(it);
					k.text = TieBreakName(it);
					return k;
				},
			} },
		});

		m.push_back(Mode {
			.id = "rareMagicJewel",
			.streams =
				{
					Stream {
						.match = [](const ItemFacts& it) { return IsRare(it) && it.nonGearKind == NonGearKind::Jewel; },
						.dir   = FillDir::ColLeft,
						.key =
							[](const ItemFacts& it) {
							SortKey k;
							k.k1   = static_cast<int64_t>(it.sortLevel);
							k.k2   = IlvlDesc(it);
							k.text = TieBreakName(it);
							return k;
						},
					},
					Stream {
						.match = [](const ItemFacts& it) { return IsMagic(it) && it.nonGearKind == NonGearKind::Jewel; },
						.dir   = FillDir::ColRight,
						.key =
							[](const ItemFacts& it) {
							SortKey k;
							k.k1   = static_cast<int64_t>(it.sortLevel);
							k.k2   = IlvlDesc(it);
							k.text = TieBreakName(it);
							return k;
						},
					},
				},
		});

		m.push_back(Mode { .id = "uniqueRingAmu",
			.streams              = { RingAmuStream(true, IsUniqueLike, true), RingAmuStream(false, IsUniqueLike, true) } });
		m.push_back(Mode { .id = "rareRingAmu", .streams = { RingAmuStream(true, IsRare, false), RingAmuStream(false, IsRare, false) } });
		m.push_back(
			Mode { .id = "magicRingAmu", .streams = { RingAmuStream(true, IsMagic, false), RingAmuStream(false, IsMagic, false) } });

		return m;
	}();
	return modes;
}

const Mode* FindMode(const std::string& id) {
	for (const auto& m : BuiltinModes()) {
		if (m.id == id) return &m;
	}
	return nullptr;
}

std::vector<std::pair<int32_t, int32_t>> ScanOrder(FillDir dir, uint32_t gridW, uint32_t gridH) {
	std::vector<std::pair<int32_t, int32_t>> order;
	order.reserve(static_cast<size_t>(gridW) * gridH);
	if (dir == FillDir::Row) {
		for (uint32_t y = 0; y < gridH; ++y)
			for (uint32_t x = 0; x < gridW; ++x) order.emplace_back(static_cast<int32_t>(x), static_cast<int32_t>(y));
	} else if (dir == FillDir::ColLeft) {
		for (uint32_t x = 0; x < gridW; ++x)
			for (uint32_t y = 0; y < gridH; ++y) order.emplace_back(static_cast<int32_t>(x), static_cast<int32_t>(y));
	} else {
		for (uint32_t xi = 0; xi < gridW; ++xi) {
			uint32_t x = gridW - 1 - xi;
			for (uint32_t y = 0; y < gridH; ++y) order.emplace_back(static_cast<int32_t>(x), static_cast<int32_t>(y));
		}
	}
	return order;
}

struct Occupancy {
	uint32_t          w = 0, h = 0;
	std::vector<bool> cells; // row-major, size w*h

	explicit Occupancy(uint32_t gw = 0, uint32_t gh = 0) : w(gw), h(gh), cells(static_cast<size_t>(gw) * gh, false) {}

	bool Free(int32_t x, int32_t y, uint32_t iw, uint32_t ih) const {
		if (x < 0 || y < 0) return false;
		if (static_cast<uint32_t>(x) + iw > w || static_cast<uint32_t>(y) + ih > h) return false;
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx)
				if (cells[static_cast<size_t>(y + dy) * w + (x + dx)]) return false;
		return true;
	}

	void Mark(int32_t x, int32_t y, uint32_t iw, uint32_t ih) {
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx) cells[static_cast<size_t>(y + dy) * w + (x + dx)] = true;
	}
};

// Best-fit within one tab, respecting a stream's own scan direction: always
// searches `order` from the start (unlike the save-editor's own
// _ssPlaceMonotonic, which never looks behind a persisted cursor), so a
// small item placed after a big one backfills whatever gap the big one left
// instead of losing that gap forever -- this plugin trades the save-editor's
// "order over density" guarantee for denser packing (fewer tabs used), by
// request (see README "Known gaps").
std::optional<std::pair<int32_t, int32_t>> PlaceBestFit(
	Occupancy& occ, const std::vector<std::pair<int32_t, int32_t>>& order, uint32_t iw, uint32_t ih) {
	for (const auto& [x, y] : order) {
		if (occ.Free(x, y, iw, ih)) {
			occ.Mark(x, y, iw, ih);
			return std::make_pair(x, y);
		}
	}
	return std::nullopt;
}

// Best-fit, non-monotonic: used only for atomic-set placement trials, which
// need to try a whole tab from the top each time. Mirrors packItemIntoGrid.
std::optional<std::pair<int32_t, int32_t>> PackBestFit(Occupancy& occ, uint32_t iw, uint32_t ih) {
	for (uint32_t y = 0; y < occ.h; ++y) {
		for (uint32_t x = 0; x < occ.w; ++x) {
			if (occ.Free(static_cast<int32_t>(x), static_cast<int32_t>(y), iw, ih)) {
				occ.Mark(static_cast<int32_t>(x), static_cast<int32_t>(y), iw, ih);
				return std::make_pair(static_cast<int32_t>(x), static_cast<int32_t>(y));
			}
		}
	}
	return std::nullopt;
}

} // namespace

std::vector<CategorySpec> DefaultLayout() {
	return {
		{ "uniqueGear", 5 },
		{ "uniqueRingAmu", 1 },
		{ "rareRingAmu", 1 },
		{ "magicRingAmu", 1 },
		{ "uniqueCharm", 1 },
		{ "magicCharm", 2 },
		{ "uniqueJewel", 1 },
		{ "rareMagicJewel", 1 },
		{ "set", 6 },
		{ "rareGear", 1 },
	};
}

bool IsKnownModeId(const std::string& id) { return FindMode(id) != nullptr; }

// --- Auto Sort --------------------------------------------------------
//
// Borrowing (mirrors _ssRunAutoSort/_ssPrepareInPlaceSort): a category is
// not capped at exactly `minTabs` tabs. That is only its *reserved* run --
// the tabs it is guaranteed even if it turns out empty. If its items do not
// fit, this borrows one more tab (from right after its reserved run, up to
// the shared stash's last tab) and retries the whole category's placement
// from scratch, growing one tab at a time until it fits or the stash truly
// has no more tabs left. The category's actual span (reserved + borrowed)
// is what advances the cursor, so every later category simply starts
// further along -- exactly the "reflow" behavior the save-editor's own
// AutoSort has always had. Unlike the save-editor, this never needs to
// physically relocate a tab's raw page content to make the borrowed tabs
// contiguous: items are moved individually via ItemService transactions, so
// "which tab index" is just a number, not a page-splicing constraint.
//
// Not yet ported: sweeping foreign items out of a target tab, and handing
// spare tabs to empty categories afterwards (the save-editor's padding
// pass) -- tracked as follow-up work (README "Known gaps vs. the
// save-editor").
SortOutcome RunAutoSort(
	const std::vector<ItemFacts>& items, const std::vector<CategorySpec>& layout, uint32_t tabCount, uint32_t gridWidth,
	uint32_t gridHeight, uint32_t startTab) {
	SortOutcome out;

	struct Attempt {
		bool               ok = false;
		std::vector<PlannedMove> moves;
		bool               matchedAnything = false;
		std::string        failMessage;
	};

	// Every item that matches ANY category in this layout gets its own fresh
	// move somewhere in THIS SAME RunAutoSort call, regardless of which step
	// currently owns the cursor -- categories just take turns computing where
	// their own items land, one at a time, in `layout` order. So an item's
	// CURRENT position (wherever compaction happened to leave it) is never a
	// real obstacle for ANY category's placement: by the time these moves
	// actually reach the game, every one of them applies. Only a genuinely
	// unclassified item -- matching no category at all, so nothing here will
	// ever move it -- is a real, permanent obstacle. Without this
	// distinction, an early category (e.g. uniqueGear, which usually runs
	// first) has to plan around the ENTIRE rest of the stash's pre-sort
	// clutter still sitting in its target tabs -- almost all of which
	// belongs to categories that simply haven't taken their turn yet -- and
	// ends up needing far more borrowed tabs than it actually needs, which
	// is exactly what a live run surfaced (a category needing dozens of
	// near-empty borrowed tabs just to get past that phantom occupancy).
	std::unordered_set<uint64_t> willBeMoved;
	willBeMoved.reserve(items.size());
	for (const auto& spec : layout) {
		const Mode* mode = FindMode(spec.id);
		if (mode == nullptr) continue; // reported as a hard error once this step is actually reached below
		for (const auto& stream : mode->streams)
			for (const auto& it : items)
				if (stream.match(it)) willBeMoved.insert(it.handle);
	}

	uint32_t cursor = std::min(startTab, tabCount);
	for (uint32_t stepIndex = 0; stepIndex < layout.size(); ++stepIndex) {
		const CategorySpec& spec = layout[stepIndex];
		const Mode*          mode = FindMode(spec.id);
		if (mode == nullptr) {
			out.error           = "unknown mode_id in layout: " + spec.id;
			out.failedStepIndex = stepIndex;
			return out;
		}

		// The reserved run clips to what is actually left in the stash rather
		// than failing outright -- the borrow loop below still gets a chance
		// with whatever tabs remain, matching _ssRunAutoSort's "take however
		// many of minTabs are available, then borrow" behavior instead of a
		// hard "not enough tabs" error before even trying.
		const uint32_t reserved = tabCount > cursor ? std::min(spec.minTabs, tabCount - cursor) : 0u;

		// Tries to place the whole category (every stream, or every set
		// group) into exactly `dests`, from a clean Occupancy grid seeded
		// with whatever already sits in those tabs. Returns ok=false without
		// having committed anything if it does not fit, so the caller can
		// grow dests and retry from scratch.
		auto tryFit = [&](const std::vector<uint32_t>& dests) -> Attempt {
			Attempt result;

			std::unordered_map<uint32_t, Occupancy> occByTab;
			for (uint32_t t : dests) occByTab.emplace(t, Occupancy(gridWidth, gridHeight));
			// Only a genuinely-unclassified item (see `willBeMoved` above)
			// blocks placement here -- anything this layout will itself
			// relocate, whether earlier-, later-, or even THIS category's
			// own items, is not a real obstacle at its old position.
			for (const auto& it : items) {
				if (it.sharedStashPage == kUnknownTab) continue;
				if (willBeMoved.count(it.handle)) continue;
				auto found = occByTab.find(it.sharedStashPage);
				if (found == occByTab.end()) continue;
				found->second.Mark(it.x, it.y, it.w, it.h);
			}

			if (!mode->atomicSets) {
				for (const auto& stream : mode->streams) {
					std::vector<const ItemFacts*> matched;
					for (const auto& it : items)
						if (stream.match(it)) matched.push_back(&it);
					if (matched.empty()) continue;
					result.matchedAnything = true;

					uint64_t totalCells = 0;
					for (const ItemFacts* it : matched) totalCells += static_cast<uint64_t>(it->w) * it->h;
					const uint64_t capacityPerTab   = static_cast<uint64_t>(gridWidth) * gridHeight;
					const uint64_t estimatedMinTabs = capacityPerTab == 0 ? 0 : (totalCells + capacityPerTab - 1) / capacityPerTab;

					std::stable_sort(matched.begin(), matched.end(), [&](const ItemFacts* a, const ItemFacts* b) {
						return KeyLess(stream.key(*a), stream.key(*b));
					});

					auto order = ScanOrder(stream.dir, gridWidth, gridHeight);

					for (const ItemFacts* it : matched) {
						// Every item re-scans `dests` from its very first tab,
						// not from wherever the previous item happened to leave
						// off. A persisted cursor (even one that only ever moves
						// forward) permanently abandons a dest tab the moment
						// ANY item fails to fit it -- so a handful of oddly-sized
						// items early in this stream's sort order can push the
						// cursor past tabs that still have plenty of room for
						// smaller items later in the same stream (or for another
						// stream's items entirely), leaving pages that look
						// mostly empty while later pages in the same category
						// still have items. Trying every dest tab, every time,
						// is what actually fills tab 0 before tab 1 is ever
						// touched -- the density the "fewer wasted cells" request
						// was asking for. This is O(items x dests) instead of
						// amortized O(dests), but dests rarely exceeds a couple
						// hundred tabs, so it stays effectively instant.
						std::optional<std::pair<int32_t, int32_t>> placed;
						uint32_t                                    chosenTab = kUnknownTab;
						for (uint32_t dest : dests) {
							placed = PlaceBestFit(occByTab.at(dest), order, it->w, it->h);
							if (placed) {
								chosenTab = dest;
								break;
							}
						}
						if (!placed) {
							result.failMessage = "category '" + spec.id + "' needs more room than " + std::to_string(dests.size())
							          + " tab(s) (" + std::to_string(matched.size()) + " item(s) in this group, ~"
							          + std::to_string(estimatedMinTabs) + " tab(s) needed at minimum)";
							return result;
						}
						result.moves.push_back({ it->handle, chosenTab, placed->first, placed->second });
					}
				}
			} else {
				// Atomic-set placement: group by setGroupName, alphabetical
				// order, each group placed whole into one dest tab, or split
				// across two consecutive dest tabs if it does not fit in one
				// (mirrors _ssPlanSort's Phase 1 / Phase 2).
				std::map<std::string, std::vector<const ItemFacts*>> groups;
				for (const auto& it : items) {
					if (!mode->streams[0].match(it)) continue;
					// match() itself now requires a non-empty setGroupName (see
					// the "set" mode's own comment) -- this is just a defensive
					// belt-and-suspenders check kept in case that ever changes.
					if (it.setGroupName.empty()) continue;
					groups[it.setGroupName].push_back(&it);
				}
				for (auto& [groupName, groupItems] : groups) {
					result.matchedAnything = true;
					bool fitted             = false;
					for (uint32_t t : dests) {
						Occupancy trial = occByTab.at(t);
						std::vector<std::pair<int32_t, int32_t>> positions;
						bool ok = true;
						for (const ItemFacts* it : groupItems) {
							auto pos = PackBestFit(trial, it->w, it->h);
							if (!pos) { ok = false; break; }
							positions.push_back(*pos);
						}
						if (ok) {
							occByTab.at(t) = trial;
							for (size_t i = 0; i < groupItems.size(); ++i)
								result.moves.push_back({ groupItems[i]->handle, t, positions[i].first, positions[i].second });
							fitted = true;
							break;
						}
					}
					if (fitted) continue;

					bool splitOk = false;
					for (size_t i = 0; i + 1 < dests.size() && !splitOk; ++i) {
						uint32_t  tA = dests[i], tB = dests[i + 1];
						Occupancy occA = occByTab.at(tA);
						Occupancy occB = occByTab.at(tB);
						std::vector<std::tuple<const ItemFacts*, uint32_t, int32_t, int32_t>> placements;
						std::vector<const ItemFacts*>                                          leftover;
						for (const ItemFacts* it : groupItems) {
							auto pos = PackBestFit(occA, it->w, it->h);
							if (pos)
								placements.emplace_back(it, tA, pos->first, pos->second);
							else
								leftover.push_back(it);
						}
						bool stillOk = true;
						for (const ItemFacts* it : leftover) {
							auto pos = PackBestFit(occB, it->w, it->h);
							if (pos)
								placements.emplace_back(it, tB, pos->first, pos->second);
							else {
								stillOk = false;
								break;
							}
						}
						if (stillOk) {
							occByTab.at(tA) = occA;
							occByTab.at(tB) = occB;
							for (auto& [it, tab, x, y] : placements) result.moves.push_back({ it->handle, tab, x, y });
							splitOk = true;
						}
					}
					if (!splitOk) {
						uint64_t groupCells = 0;
						for (const ItemFacts* it : groupItems) groupCells += static_cast<uint64_t>(it->w) * it->h;
						result.failMessage = "set group '" + groupName + "' (" + std::to_string(groupItems.size()) + " item(s), "
						          + std::to_string(groupCells) + " cell(s)) does not fit across two reserved set tabs";
						return result;
					}
				}
			}

			result.ok = true;
			return result;
		};

		uint32_t borrowed = 0;
		Attempt  attempt;
		while (true) {
			std::vector<uint32_t> dests;
			for (uint32_t t = cursor; t < cursor + reserved + borrowed; ++t) dests.push_back(t);
			attempt = tryFit(dests);
			if (attempt.ok) {
				break;
			}
			if (cursor + reserved + borrowed >= tabCount) {
				// No more tabs left in the whole shared stash to borrow.
				break;
			}
			++borrowed;
		}

		if (!attempt.ok) {
			out.error = attempt.failMessage.empty()
			          ? ("category '" + spec.id + "' has no shared-stash tabs available to sort into")
			          : (attempt.failMessage + (borrowed > 0
			                ? (" (already borrowed " + std::to_string(borrowed) + " extra tab(s) beyond its " + std::to_string(spec.minTabs)
			                   + "-tab reservation; the shared stash has no more tabs left to give it)")
			                : std::string()));
			out.failedStepIndex = stepIndex;
			return out;
		}

		out.moves.insert(out.moves.end(), attempt.moves.begin(), attempt.moves.end());
		if (!attempt.matchedAnything) out.skippedCategories.push_back(spec.id);

		{
			const uint32_t span = reserved + borrowed;
			std::string    line = spec.id + ": ";
			if (span == 0) {
				line += "no tabs available";
			} else {
				line += "tabs [" + std::to_string(cursor) + ".." + std::to_string(cursor + span - 1) + "] (" + std::to_string(reserved)
				     + " reserved + " + std::to_string(borrowed) + " borrowed)";
			}
			line += ", " + std::to_string(attempt.moves.size()) + " item(s) placed";
			out.categoryLog.push_back(std::move(line));

			out.spans.push_back(
				{ .id = spec.id, .start = cursor, .end = cursor + span, .tabsUsed = span, .matchedAnything = attempt.matchedAnything });
		}

		cursor += reserved + borrowed;
	}

	out.ok          = true;
	out.tabsUsed    = cursor;
	out.itemsSorted = static_cast<uint32_t>(out.moves.size());
	return out;
}

namespace {

std::map<uint32_t, std::vector<const ItemFacts*>> GroupByTab(const std::vector<ItemFacts>& items) {
	std::map<uint32_t, std::vector<const ItemFacts*>> byTab;
	for (const auto& it : items) {
		if (it.sharedStashPage == kUnknownTab) continue;
		byTab[it.sharedStashPage].push_back(&it);
	}
	return byTab;
}

} // namespace

ShiftOutcome MoveBlankTabAfter(const std::vector<ItemFacts>& items, uint32_t afterTab, uint32_t tabCount) {
	ShiftOutcome out;
	auto         byTab = GroupByTab(items);

	uint32_t blank = kUnknownTab;
	for (uint32_t t = afterTab + 1; t < tabCount; ++t) {
		if (byTab.find(t) == byTab.end()) {
			blank = t;
			break;
		}
	}
	if (blank == kUnknownTab) {
		out.error = "no empty tab found after the current page";
		return out;
	}

	// Shift tabs (afterTab+1 .. blank-1) forward by one slot, last first, so
	// the gap walks from `blank` down to afterTab+1 without ever colliding
	// with a not-yet-moved tab.
	for (uint32_t t = blank; t > afterTab + 1; --t) {
		uint32_t from = t - 1;
		auto     found = byTab.find(from);
		if (found == byTab.end()) continue;
		for (const ItemFacts* it : found->second) out.moves.push_back({ it->handle, t, it->x, it->y });
	}

	out.ok       = true;
	out.blankTab = afterTab + 1;
	return out;
}

ShiftOutcome PushBlankTabToEnd(const std::vector<ItemFacts>& items, uint32_t currentTab, uint32_t tabCount) {
	ShiftOutcome out;
	auto         byTab = GroupByTab(items);

	if (byTab.find(currentTab) != byTab.end()) {
		out.error = "current page is not empty";
		return out;
	}
	if (currentTab >= tabCount) {
		out.error = "current page is out of range";
		return out;
	}

	for (uint32_t t = currentTab + 1; t < tabCount; ++t) {
		auto found = byTab.find(t);
		if (found == byTab.end()) continue;
		for (const ItemFacts* it : found->second) out.moves.push_back({ it->handle, t - 1, it->x, it->y });
	}

	out.ok       = true;
	out.blankTab = tabCount - 1;
	return out;
}

std::vector<std::string> DetectCollisions(
	const std::vector<ItemFacts>& items, const std::vector<PlannedMove>& moves, uint32_t gridWidth, uint32_t gridHeight) {
	std::vector<std::string> out;

	std::unordered_map<uint64_t, const ItemFacts*> byHandle;
	byHandle.reserve(items.size());
	for (const auto& it : items) byHandle.emplace(it.handle, &it);

	// A multi-phase plan (RunAutoSortWithPadding: compact, sort, pad,
	// blank-at-start) can move the same item more than once -- e.g. compact
	// relabels its tab, then the category sort places it somewhere else
	// entirely. Only its LAST move is where it actually ends up, so that is
	// the only one worth checking for a collision; keying by handle and
	// overwriting on each occurrence keeps exactly that.
	std::unordered_map<uint64_t, PlannedMove> finalMoveByHandle;
	finalMoveByHandle.reserve(moves.size());
	for (const auto& mv : moves) finalMoveByHandle[mv.handle] = mv;

	// Moves this plan makes don't need pre-existing occupancy seeded (a move
	// is only ever planned into a cell RunAutoSort believed was free), but a
	// mis-classified item's real w/h could make that belief wrong -- so this
	// replays purely from each item's final destination plus its *live*
	// footprint, which is exactly what would catch that class of bug.
	std::unordered_map<uint32_t, Occupancy> occByTab;
	auto                                    tabGrid = [&](uint32_t tab) -> Occupancy& {
        auto found = occByTab.find(tab);
        if (found != occByTab.end()) return found->second;
        return occByTab.emplace(tab, Occupancy(gridWidth, gridHeight)).first->second;
	};

	for (const auto& entry : finalMoveByHandle) {
		const PlannedMove& mv = entry.second;
		const auto found = byHandle.find(mv.handle);
		if (found == byHandle.end()) {
			out.push_back("move for unknown handle " + std::to_string(mv.handle) + " -> tab " + std::to_string(mv.destTab));
			continue;
		}
		const ItemFacts& fact = *found->second;
		Occupancy&        grid = tabGrid(mv.destTab);
		if (!grid.Free(mv.destX, mv.destY, fact.w, fact.h)) {
			char codeHex[16];
			std::snprintf(codeHex, sizeof(codeHex), "%08X", fact.code);
			out.push_back(
				std::string("COLLISION: item code 0x") + codeHex + " (" + std::to_string(fact.w) + "x" + std::to_string(fact.h)
				+ ") -> tab " + std::to_string(mv.destTab) + " @ (" + std::to_string(mv.destX) + "," + std::to_string(mv.destY)
				+ ") is already occupied");
			if (out.size() >= 20) break;
			continue;
		}
		grid.Mark(mv.destX, mv.destY, fact.w, fact.h);
	}

	return out;
}

namespace {

// Applies `moves` onto a working copy of ItemFacts in place, keyed by
// handle -- used between RunAutoSortWithPadding's phases so each one plans
// against exactly where the previous phase left everything, without ever
// touching the live game until the caller applies the final combined list.
void ApplyMovesToWorkingSet(std::vector<ItemFacts>& items, const std::vector<PlannedMove>& moves) {
	if (moves.empty()) return;
	std::unordered_map<uint64_t, ItemFacts*> byHandle;
	byHandle.reserve(items.size());
	for (auto& it : items) byHandle.emplace(it.handle, &it);
	for (const auto& mv : moves) {
		auto found = byHandle.find(mv.handle);
		if (found == byHandle.end()) continue;
		found->second->sharedStashPage = mv.destTab;
		found->second->x               = mv.destX;
		found->second->y               = mv.destY;
	}
}

// Shared rotation core behind AddBlankTabsAtStart and the padding pass's
// per-category insertion (MoveBlankTabAfter/PushBlankTabToEnd above have
// their own near-identical copies predating this helper and are left as-is
// to avoid touching already-working, already-tested code): finds the
// nearest empty tab at or after `searchFrom` and slides it to land exactly
// at `dst` (dst <= searchFrom), shifting every tab in between forward by one
// slot -- last first, so the gap walks down to `dst` without ever colliding
// with a not-yet-moved tab. Picking the *nearest* empty tab (rather than the
// save-editor's own "farthest from the end" rule) minimises how many items
// this rotation has to move, which matters here since a "tab move" is real
// per-item Move operations, not a save-editor byte-page swap. Returns
// std::nullopt if the stash has no empty tab left at or after `searchFrom`.
std::optional<std::vector<PlannedMove>> RotateEmptyTabTo(
	const std::vector<ItemFacts>& items, uint32_t dst, uint32_t searchFrom, uint32_t tabCount) {
	auto     byTab = GroupByTab(items);
	uint32_t src    = kUnknownTab;
	for (uint32_t t = searchFrom; t < tabCount; ++t) {
		if (byTab.find(t) == byTab.end()) {
			src = t;
			break;
		}
	}
	if (src == kUnknownTab) return std::nullopt;

	std::vector<PlannedMove> moves;
	for (uint32_t t = src; t > dst; --t) {
		uint32_t from  = t - 1;
		auto     found = byTab.find(from);
		if (found == byTab.end()) continue;
		for (const ItemFacts* it : found->second) moves.push_back({ it->handle, t, it->x, it->y });
	}
	return moves;
}

double TabFillPercent(const std::vector<ItemFacts>& items, uint32_t tab, uint32_t gridWidth, uint32_t gridHeight) {
	uint64_t cells = 0;
	for (const auto& it : items) {
		if (it.sharedStashPage == tab) cells += static_cast<uint64_t>(it.w) * it.h;
	}
	const uint64_t capacity = static_cast<uint64_t>(gridWidth) * gridHeight;
	return capacity == 0 ? 0.0 : (static_cast<double>(cells) / static_cast<double>(capacity)) * 100.0;
}

// True when category k's run ends in a tab fuller than its threshold --
// mirrors _ssWantsEmptyPage. spec's own addEmptyPageWhen wins over the
// global rule when set.
bool WantsEmptyPage(
	const CategorySpan& span, const CategorySpec& spec, const EmptyPageRule& globalRule, const std::vector<ItemFacts>& items,
	uint32_t gridWidth, uint32_t gridHeight) {
	const EmptyPageRule& rule = spec.hasEmptyPageOverride ? spec.emptyPageOverride : globalRule;
	if (!rule.enabled) return false;
	if (span.tabsUsed == 0 || span.end == 0) return false; // nothing sorted, no run to measure
	const uint32_t lastTab = span.end - 1;
	return TabFillPercent(items, lastTab, gridWidth, gridHeight) > rule.percentThreshold;
}

struct PaddingOutcome {
	std::vector<PlannedMove>  moves;
	std::vector<CategorySpan> spans;
	uint32_t                  added = 0;
};

// Pass 2 only (save-editor's addEmptyPageWhen "breathing page" -- see the
// header comment on AutoSortConfig for why pass 1, the quota-shortfall
// redistribution, isn't ported): for each category whose run ended in a tab
// fuller than its threshold, inserts ONE extra blank tab right after that
// category's run, donor pulled from behind the whole layout, pushing every
// later category's tabs one slot along. Mirrors _ssPadAutoLayout's pass 2 /
// _ssWantsEmptyPage.
PaddingOutcome PadAutoLayoutBreathingRoom(
	std::vector<ItemFacts> workingItems, std::vector<CategorySpan> spans, const std::vector<CategorySpec>& layout,
	const EmptyPageRule& globalRule, uint32_t tabCount, uint32_t gridWidth, uint32_t gridHeight) {
	PaddingOutcome out;
	if (spans.empty() || spans.size() != layout.size()) {
		out.spans = spans;
		return out;
	}

	for (size_t k = 0; k < spans.size(); ++k) {
		if (!WantsEmptyPage(spans[k], layout[k], globalRule, workingItems, gridWidth, gridHeight)) continue;

		const uint32_t layoutEnd = spans.back().end;
		auto           rotated   = RotateEmptyTabTo(workingItems, spans[k].end, layoutEnd, tabCount);
		if (!rotated) break; // stash is out of empty tabs; stop handing out breathing room

		ApplyMovesToWorkingSet(workingItems, *rotated);
		out.moves.insert(out.moves.end(), rotated->begin(), rotated->end());

		spans[k].end += 1;
		spans[k].tabsUsed += 1;
		for (size_t j = k + 1; j < spans.size(); ++j) {
			spans[j].start += 1;
			spans[j].end += 1;
		}
		out.added += 1;
	}

	out.spans = spans;
	return out;
}

// Mirrors _ssAddBlankTabsAtStart: pulls `n` empty tabs up to sit immediately
// after `startTab`, before the whole layout. Donors only ever come from
// behind `layoutEnd` -- never from inside the layout itself. Returns how
// many it managed, which is less than `n` only when the stash ran out of
// empty tabs to give.
uint32_t AddBlankTabsAtStart(
	std::vector<ItemFacts> workingItems, std::vector<PlannedMove>& outMoves, uint32_t n, uint32_t startTab, uint32_t layoutEnd,
	uint32_t tabCount) {
	uint32_t added = 0;
	for (uint32_t k = 0; k < n; ++k) {
		auto rotated = RotateEmptyTabTo(workingItems, startTab + added, layoutEnd + added, tabCount);
		if (!rotated) break;
		ApplyMovesToWorkingSet(workingItems, *rotated);
		outMoves.insert(outMoves.end(), rotated->begin(), rotated->end());
		++added;
	}
	return added;
}

} // namespace

std::vector<PlannedMove> CompactEmptyTabsToEnd(const std::vector<ItemFacts>& items, uint32_t startTab, uint32_t tabCount) {
	auto                   byTab = GroupByTab(items);
	std::vector<uint32_t> nonEmpty, empty;
	for (uint32_t t = startTab; t < tabCount; ++t) {
		(byTab.find(t) == byTab.end() ? empty : nonEmpty).push_back(t);
	}

	std::vector<uint32_t> order;
	order.reserve(nonEmpty.size() + empty.size());
	order.insert(order.end(), nonEmpty.begin(), nonEmpty.end());
	order.insert(order.end(), empty.begin(), empty.end());

	std::vector<PlannedMove> moves;
	for (size_t i = 0; i < order.size(); ++i) {
		const uint32_t oldTab = order[i];
		const uint32_t newTab = startTab + static_cast<uint32_t>(i);
		if (oldTab == newTab) continue;
		auto found = byTab.find(oldTab);
		if (found == byTab.end()) continue; // an empty tab has nothing to relabel
		for (const ItemFacts* it : found->second) moves.push_back({ it->handle, newTab, it->x, it->y });
	}
	return moves;
}

AutoSortPlan RunAutoSortWithPadding(const std::vector<ItemFacts>& items, const AutoSortConfig& cfg) {
	AutoSortPlan            plan;
	std::vector<ItemFacts>  working = items;

	const uint32_t startTab = std::min(cfg.startTab, cfg.tabCount);

	if (cfg.compactEmptyTabsFirst) {
		auto compactMoves = CompactEmptyTabsToEnd(working, startTab, cfg.tabCount);
		ApplyMovesToWorkingSet(working, compactMoves);
		plan.moves.insert(plan.moves.end(), compactMoves.begin(), compactMoves.end());
		plan.compactMoveCount = static_cast<uint32_t>(compactMoves.size());
	}

	SortOutcome sortOutcome = RunAutoSort(working, cfg.layout, cfg.tabCount, cfg.gridWidth, cfg.gridHeight, startTab);
	plan.categoryLog        = sortOutcome.categoryLog;
	plan.skippedCategories  = sortOutcome.skippedCategories;
	if (!sortOutcome.ok) {
		plan.ok             = false;
		plan.error           = sortOutcome.error;
		plan.failedStepIndex = sortOutcome.failedStepIndex;
		return plan;
	}
	ApplyMovesToWorkingSet(working, sortOutcome.moves);
	plan.moves.insert(plan.moves.end(), sortOutcome.moves.begin(), sortOutcome.moves.end());
	plan.itemsSorted = sortOutcome.itemsSorted;
	plan.tabsUsed    = sortOutcome.tabsUsed;

	std::vector<CategorySpan> spans = sortOutcome.spans;
	if (!spans.empty()) {
		auto padding =
			PadAutoLayoutBreathingRoom(working, spans, cfg.layout, cfg.addEmptyPageWhen, cfg.tabCount, cfg.gridWidth, cfg.gridHeight);
		ApplyMovesToWorkingSet(working, padding.moves);
		plan.moves.insert(plan.moves.end(), padding.moves.begin(), padding.moves.end());
		plan.breathingTabsAdded = padding.added;
		spans                   = padding.spans;
	}

	if (cfg.blankTabsAtStart > 0) {
		const uint32_t           layoutEnd = spans.empty() ? startTab : spans.back().end;
		std::vector<PlannedMove> blankMoves;
		plan.blankTabsAtStartAdded = AddBlankTabsAtStart(working, blankMoves, cfg.blankTabsAtStart, startTab, layoutEnd, cfg.tabCount);
		plan.moves.insert(plan.moves.end(), blankMoves.begin(), blankMoves.end());
	}

	// Collapse to one move per item, keeping only each handle's FINAL move.
	// This plan concatenates every phase (compact, sort, pad,
	// blank-at-start), and the same item routinely gets moved more than
	// once across them -- e.g. compacted into a closed-up tab, then placed
	// somewhere else entirely by the category sort. Sending the
	// now-superseded earlier move alongside the final one isn't just wasted
	// work: ExistingItemTransaction rejects the WHOLE chunk it's in with
	// Result::InvalidArgument if any item handle appears more than once, so
	// without this an otherwise-correct plan can fail to commit anything at
	// all. DetectCollisions already dedupes the same way for its own
	// self-check (see its finalMoveByHandle) -- this applies that same rule
	// to what actually gets sent to the game.
	{
		std::unordered_map<uint64_t, size_t> lastIndexByHandle;
		lastIndexByHandle.reserve(plan.moves.size());
		for (size_t i = 0; i < plan.moves.size(); ++i) lastIndexByHandle[plan.moves[i].handle] = i;

		std::vector<PlannedMove> finalMoves;
		finalMoves.reserve(lastIndexByHandle.size());
		for (size_t i = 0; i < plan.moves.size(); ++i) {
			if (lastIndexByHandle.at(plan.moves[i].handle) == i) finalMoves.push_back(plan.moves[i]);
		}
		plan.moves = std::move(finalMoves);
	}

	plan.ok = true;
	return plan;
}

}

