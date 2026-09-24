#include "sort_engine.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_map>

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
				   .match = [](const ItemFacts& it) { return IsSetQuality(it); },
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

// Monotonic first-fit: scans `order` forward from *cursor, never backtracks
// even if an earlier cell would later become free -- mirrors
// _ssPlaceMonotonic's "order over density" rule so items already placed keep
// their relative order.
std::optional<std::pair<int32_t, int32_t>> PlaceMonotonic(
	Occupancy& occ, const std::vector<std::pair<int32_t, int32_t>>& order, size_t& cursor, uint32_t iw, uint32_t ih) {
	while (cursor < order.size()) {
		auto [x, y] = order[cursor];
		if (occ.Free(x, y, iw, ih)) {
			occ.Mark(x, y, iw, ih);
			++cursor;
			return std::make_pair(x, y);
		}
		++cursor;
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
// v1 scope note (see README "Known gaps vs. the save-editor"): unlike
// _ssRunAutoSort, this port does not yet borrow extra empty tabs when a
// category overflows its quota, does not sweep foreign items out of a
// target tab, and does not hand spare tabs to empty categories afterwards.
// A category that does not fit in exactly `minTabs` tabs fails the whole
// run with a clear error instead of silently reflowing the layout. This
// covers the common case (stash roughly following the canonical layout
// already, or being sorted for the first time with reasonable headroom);
// the borrow/sweep/pad behaviors are tracked as follow-up work.
SortOutcome RunAutoSort(
	const std::vector<ItemFacts>& items, const std::vector<CategorySpec>& layout, uint32_t tabCount, uint32_t gridWidth,
	uint32_t gridHeight) {
	SortOutcome out;

	uint32_t cursor = 0;
	for (uint32_t stepIndex = 0; stepIndex < layout.size(); ++stepIndex) {
		const CategorySpec& spec = layout[stepIndex];
		const Mode*          mode = FindMode(spec.id);
		if (mode == nullptr) {
			out.error           = "unknown mode_id in layout: " + spec.id;
			out.failedStepIndex = stepIndex;
			return out;
		}

		if (cursor + spec.minTabs > tabCount) {
			out.error           = "not enough shared-stash tabs for category '" + spec.id + "' (needs " + std::to_string(spec.minTabs)
			          + " starting at tab " + std::to_string(cursor) + ", stash has " + std::to_string(tabCount) + ")";
			out.failedStepIndex = stepIndex;
			return out;
		}
		std::vector<uint32_t> dests;
		for (uint32_t t = cursor; t < cursor + spec.minTabs; ++t) dests.push_back(t);

		std::unordered_map<uint32_t, Occupancy> occByTab;
		for (uint32_t t : dests) occByTab.emplace(t, Occupancy(gridWidth, gridHeight));
		// Existing items already sitting in a dest tab (from an earlier
		// category's pass, or leftovers -- see the v1 scope note above) stay
		// where they are and occupy their cells.
		for (const auto& it : items) {
			if (it.sharedStashPage == kUnknownTab) continue;
			auto found = occByTab.find(it.sharedStashPage);
			if (found == occByTab.end()) continue;
			found->second.Mark(it.x, it.y, it.w, it.h);
		}

		bool matchedAnything = false;

		if (!mode->atomicSets) {
			for (const auto& stream : mode->streams) {
				std::vector<const ItemFacts*> matched;
				for (const auto& it : items)
					if (stream.match(it)) matched.push_back(&it);
				if (matched.empty()) continue;
				matchedAnything = true;

				std::stable_sort(matched.begin(), matched.end(), [&](const ItemFacts* a, const ItemFacts* b) {
					return KeyLess(stream.key(*a), stream.key(*b));
				});

				auto             order = ScanOrder(stream.dir, gridWidth, gridHeight);
				size_t            destPos = 0;
				std::vector<size_t> cursorsByDest(dests.size(), 0);

				for (const ItemFacts* it : matched) {
					std::optional<std::pair<int32_t, int32_t>> placed;
					while (destPos < dests.size()) {
						placed = PlaceMonotonic(occByTab.at(dests[destPos]), order, cursorsByDest[destPos], it->w, it->h);
						if (placed) break;
						++destPos;
					}
					if (!placed) {
						out.error = "category '" + spec.id + "' needs more room than its " + std::to_string(spec.minTabs) + " tab(s)";
						out.failedStepIndex = stepIndex;
						return out;
					}
					out.moves.push_back({ it->handle, dests[destPos], placed->first, placed->second });
				}
			}
		} else {
			// Atomic-set placement: group by setGroupName, alphabetical order,
			// each group placed whole into one dest tab, or split across two
			// consecutive dest tabs if it does not fit in one (mirrors
			// _ssPlanSort's Phase 1 / Phase 2).
			std::map<std::string, std::vector<const ItemFacts*>> groups;
			for (const auto& it : items) {
				if (!mode->streams[0].match(it)) continue;
				if (it.setGroupName.empty()) continue;
				groups[it.setGroupName].push_back(&it);
			}
			for (auto& [groupName, groupItems] : groups) {
				matchedAnything = true;
				bool fitted     = false;
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
							out.moves.push_back({ groupItems[i]->handle, t, positions[i].first, positions[i].second });
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
						for (auto& [it, tab, x, y] : placements) out.moves.push_back({ it->handle, tab, x, y });
						splitOk = true;
					}
				}
				if (!splitOk) {
					out.error = "set group '" + groupName + "' does not fit in the reserved set tabs";
					out.failedStepIndex = stepIndex;
					return out;
				}
			}
		}

		if (!matchedAnything) out.skippedCategories.push_back(spec.id);
		cursor += spec.minTabs;
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

}
