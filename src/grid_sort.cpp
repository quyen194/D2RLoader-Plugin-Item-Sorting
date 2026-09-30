#include "grid_sort.hpp"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Quyen194ItemSorting::Grid {

bool ParseFillDirection(const std::string& text, FillDirection* out) {
	if (text == "columns") { *out = FillDirection::Columns; return true; }
	if (text == "rows") { *out = FillDirection::Rows; return true; }
	return false;
}

bool ParseCategoryBreak(const std::string& text, CategoryBreak* out) {
	if (text == "none") { *out = CategoryBreak::None; return true; }
	if (text == "band") { *out = CategoryBreak::Band; return true; }
	return false;
}

bool ParseOverflow(const std::string& text, Overflow* out) {
	if (text == "compact") { *out = Overflow::Compact; return true; }
	if (text == "abort") { *out = Overflow::Abort; return true; }
	return false;
}

bool ParseAnchor(const std::string& text, Anchor* out) {
	if (text == "topLeft") { *out = Anchor::TopLeft; return true; }
	if (text == "topRight") { *out = Anchor::TopRight; return true; }
	if (text == "bottomLeft") { *out = Anchor::BottomLeft; return true; }
	if (text == "bottomRight") { *out = Anchor::BottomRight; return true; }
	return false;
}

const char* ToString(FillDirection v) { return v == FillDirection::Columns ? "columns" : "rows"; }
const char* ToString(CategoryBreak v) { return v == CategoryBreak::Band ? "band" : "none"; }
const char* ToString(Overflow v) { return v == Overflow::Compact ? "compact" : "abort"; }
const char* ToString(Anchor v) {
	switch (v) {
		case Anchor::TopLeft: return "topLeft";
		case Anchor::TopRight: return "topRight";
		case Anchor::BottomLeft: return "bottomLeft";
		case Anchor::BottomRight: return "bottomRight";
	}
	return "topLeft";
}

namespace {

std::string RectText(const Rect& r) {
	char buf[96];
	std::snprintf(buf, sizeof(buf), "{ x = %d, y = %d, w = %d, h = %d }", r.x, r.y, r.w, r.h);
	return buf;
}

bool RectInside(const Rect& r, uint32_t w, uint32_t h) {
	return r.w >= 1 && r.h >= 1 && r.x >= 0 && r.y >= 0 && r.x + r.w <= static_cast<int32_t>(w) && r.y + r.h <= static_cast<int32_t>(h);
}

bool RectsOverlap(const Rect& a, const Rect& b) {
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

std::string EntryLabel(const Entry& e) {
	std::string s;
	for (const auto& id : e.modeIds) {
		if (!s.empty()) s += "+";
		s += id;
	}
	return s.empty() ? std::string("(no mode)") : s;
}

// Mode membership for the single-grid containers. Differs from the
// shared-stash engine in one way only: gear modes never claim an item with
// GearSlot::None. The embedded gear table marks every "other" misc.txt item
// (potions, quest items, Reimagined's orbs/stacks/dyes/elixirs) as
// isGear=true with GearSlot::None -- harmless across 200 shared tabs, but in
// one small grid it would drop potions into the middle of a gear block.
bool GridModeMatches(const std::string& id, const ItemFacts& it) {
	if (IsGearModeId(id) && it.gearSlot == GearSlot::None) return false;
	return ModeMatchesItem(id, it);
}

struct Occupancy {
	uint32_t          w = 0;
	uint32_t          h = 0;
	std::vector<bool> used; // row-major

	Occupancy(uint32_t width, uint32_t height) : w(width), h(height), used(static_cast<size_t>(width) * height, false) {}

	bool At(int32_t x, int32_t y) const { return used[static_cast<size_t>(y) * w + static_cast<size_t>(x)]; }
	void Mark(int32_t x, int32_t y, int32_t iw, int32_t ih) {
		for (int32_t yy = y; yy < y + ih; ++yy)
			for (int32_t xx = x; xx < x + iw; ++xx) used[static_cast<size_t>(yy) * w + static_cast<size_t>(xx)] = true;
	}
	bool Free(int32_t x, int32_t y, int32_t iw, int32_t ih, const std::vector<bool>* forbidden) const {
		for (int32_t yy = y; yy < y + ih; ++yy) {
			for (int32_t xx = x; xx < x + iw; ++xx) {
				const size_t idx = static_cast<size_t>(yy) * w + static_cast<size_t>(xx);
				if (used[idx]) return false;
				if (forbidden != nullptr && (*forbidden)[idx]) return false;
			}
		}
		return true;
	}
};

// Finds the first cell (in anchor-relative scan order) inside `region`
// where an iw x ih item fits. The scan's primary axis is the distance of the
// item's near edge from the anchor side -- columns for FillDirection::
// Columns, rows for Rows -- starting at `bandStart`; the secondary axis is
// the distance from the other anchor side. So a topLeft/columns scan fills
// column 0 top to bottom, then column 1, ...; a bottomRight/columns scan
// fills the right-most column bottom to top, then the one left of it, ...
// `extent` receives the far edge (primary axis) of the placed item, for the
// band bookkeeping.
std::optional<std::pair<int32_t, int32_t>> FindSpot(
	const Occupancy& occ, const std::vector<bool>* forbidden, const Rect& region, Anchor anchor, FillDirection fill, int32_t bandStart,
	int32_t iw, int32_t ih, int32_t* extent) {
	const bool right  = anchor == Anchor::TopRight || anchor == Anchor::BottomRight;
	const bool bottom = anchor == Anchor::BottomLeft || anchor == Anchor::BottomRight;
	const bool cols   = fill == FillDirection::Columns;

	const int32_t primaryLen   = cols ? region.w : region.h;
	const int32_t secondaryLen = cols ? region.h : region.w;
	const int32_t primarySize  = cols ? iw : ih;
	const int32_t secondarySize = cols ? ih : iw;

	for (int32_t p = std::max<int32_t>(0, bandStart); p + primarySize <= primaryLen; ++p) {
		for (int32_t s = 0; s + secondarySize <= secondaryLen; ++s) {
			const int32_t colDist = cols ? p : s;
			const int32_t rowDist = cols ? s : p;
			const int32_t x       = right ? region.x + region.w - colDist - iw : region.x + colDist;
			const int32_t y       = bottom ? region.y + region.h - rowDist - ih : region.y + rowDist;
			if (occ.Free(x, y, iw, ih, forbidden)) {
				if (extent != nullptr) *extent = p + primarySize;
				return std::make_pair(x, y);
			}
		}
	}
	return std::nullopt;
}

struct Assignment {
	std::vector<std::vector<const ItemFacts*>> perEntry; // index = layout entry
	std::vector<const ItemFacts*>              fixed;
};

struct Attempt {
	bool                                         ok = false;
	std::string                                  error;
	std::vector<std::pair<const ItemFacts*, std::pair<int32_t, int32_t>>> placed;
	std::vector<std::string>                     entryLog;
};

Attempt TryArrange(const Config& cfg, const Assignment& assign, const Occupancy& base, CategoryBreak brk) {
	Attempt attempt;
	Occupancy occ = base;

	const Rect whole { 0, 0, static_cast<int32_t>(cfg.width), static_cast<int32_t>(cfg.height) };
	std::vector<bool> zoneMask(static_cast<size_t>(cfg.width) * cfg.height, false);
	for (const auto& e : cfg.layout) {
		if (!e.hasZone) continue;
		for (int32_t y = e.zone.y; y < e.zone.y + e.zone.h; ++y)
			for (int32_t x = e.zone.x; x < e.zone.x + e.zone.w; ++x) zoneMask[static_cast<size_t>(y) * cfg.width + static_cast<size_t>(x)] = true;
	}

	std::vector<size_t> order;
	for (size_t i = 0; i < cfg.layout.size(); ++i)
		if (cfg.layout[i].hasZone) order.push_back(i);
	for (size_t i = 0; i < cfg.layout.size(); ++i)
		if (!cfg.layout[i].hasZone) order.push_back(i);

	int32_t bandStart[4] = { 0, 0, 0, 0 }; // per Anchor

	for (size_t idx : order) {
		const Entry& entry = cfg.layout[idx];
		std::vector<const ItemFacts*> items = assign.perEntry[idx];

		std::vector<std::string> keys = entry.sortKeys;
		if (keys.empty() && !entry.modeIds.empty()) keys = DefaultSortKeysForMode(entry.modeIds.front());
		const auto less = MakeItemOrder(keys, entry.gearGroups);
		std::stable_sort(items.begin(), items.end(), [&less](const ItemFacts* a, const ItemFacts* b) { return less(*a, *b); });

		const int  anchorIdx   = static_cast<int>(entry.anchor);
		const bool useBand     = !entry.hasZone && brk == CategoryBreak::Band;
		const int32_t start    = useBand ? bandStart[anchorIdx] : 0;
		int32_t    maxExtent   = start;
		uint32_t   inZone      = 0;
		uint32_t   spilled     = 0;

		for (const ItemFacts* it : items) {
			const int32_t iw = static_cast<int32_t>(it->w);
			const int32_t ih = static_cast<int32_t>(it->h);
			std::optional<std::pair<int32_t, int32_t>> spot;
			int32_t extent = 0;
			if (entry.hasZone) {
				spot = FindSpot(occ, nullptr, entry.zone, entry.anchor, cfg.fill, 0, iw, ih, nullptr);
				if (spot) {
					++inZone;
				} else {
					spot = FindSpot(occ, &zoneMask, whole, entry.anchor, cfg.fill, 0, iw, ih, nullptr);
					if (spot) ++spilled;
				}
			} else {
				spot = FindSpot(occ, &zoneMask, whole, entry.anchor, cfg.fill, start, iw, ih, &extent);
				if (spot) maxExtent = std::max(maxExtent, extent);
			}
			if (!spot) {
				char buf[256];
				std::snprintf(buf, sizeof(buf), "[%s] layout entry %zu (%s): no room left for a %dx%d item (code 0x%08X)", cfg.name.c_str(),
					idx + 1, EntryLabel(entry).c_str(), iw, ih, it->code);
				attempt.error = buf;
				return attempt;
			}
			occ.Mark(spot->first, spot->second, iw, ih);
			attempt.placed.push_back({ it, *spot });
		}

		if (useBand && !items.empty()) bandStart[anchorIdx] = maxExtent;

		char buf[256];
		if (entry.hasZone) {
			std::snprintf(buf, sizeof(buf), "[%s] entry %zu (%s, zone %s, %s): %zu item(s), %u in zone, %u spilled", cfg.name.c_str(), idx + 1,
				EntryLabel(entry).c_str(), RectText(entry.zone).c_str(), ToString(entry.anchor), items.size(), inZone, spilled);
		} else {
			std::snprintf(buf, sizeof(buf), "[%s] entry %zu (%s, %s): %zu item(s)", cfg.name.c_str(), idx + 1, EntryLabel(entry).c_str(),
				ToString(entry.anchor), items.size());
		}
		attempt.entryLog.push_back(buf);
	}

	attempt.ok = true;
	return attempt;
}

} // namespace

Config DefaultConfig(const std::string& section) {
	Config cfg;
	cfg.name = section;
	const std::vector<std::string> allGear = { "helm", "tors", "belt", "glov", "boot", "shld", "axe", "swor", "knif", "blun", "scep", "wand",
		"staf", "orb", "spea", "pole", "jave", "bow", "bowq", "xbow", "xboq", "h2h", "weap" };

	if (section == "inventory") {
		cfg.width         = 10;
		cfg.height        = 8;
		cfg.fill          = FillDirection::Columns;
		cfg.categoryBreak = CategoryBreak::None;
		cfg.onOverflow    = Overflow::Abort;
		cfg.keepInPlace   = { "uniqueCharm", "magicCharm" };
		cfg.layout.push_back(Entry { .modeIds = { "tool" }, .anchor = Anchor::TopRight, .sortKeys = { "-area", "name" } });
		cfg.layout.push_back(Entry { .modeIds = { "unclassified" }, .anchor = Anchor::TopLeft, .sortKeys = { "-area", "quality", "name" } });
		return cfg;
	}
	if (section == "cube") {
		cfg.width         = 12;
		cfg.height        = 8;
		cfg.fill          = FillDirection::Rows;
		cfg.categoryBreak = CategoryBreak::None;
		cfg.onOverflow    = Overflow::Compact;
		cfg.layout.push_back(Entry { .modeIds = { "rune" }, .sortKeys = { "runeNumber" } });
		cfg.layout.push_back(Entry { .modeIds = { "gem" }, .sortKeys = { "gemType", "-gemGrade" } });
		cfg.layout.push_back(Entry { .modeIds = { "unclassified" }, .sortKeys = { "-area", "name" } });
		return cfg;
	}

	// personal_stash (also the fallback for an unknown section name)
	cfg.width         = 16;
	cfg.height        = 13;
	cfg.fill          = FillDirection::Columns;
	cfg.categoryBreak = CategoryBreak::Band;
	cfg.onOverflow    = Overflow::Compact;
	cfg.layout.push_back(Entry {
		.modeIds  = { "rune" },
		.anchor   = Anchor::TopRight,
		.hasZone  = true,
		.zone     = Rect { 14, 0, 2, 13 },
		.sortKeys = { "runeNumber" },
	});
	cfg.layout.push_back(Entry { .modeIds = { "gem" }, .sortKeys = { "gemType", "-gemGrade" } });
	cfg.layout.push_back(Entry { .modeIds = { "uniqueCharm", "magicCharm" }, .sortKeys = { "-size", "quality", "reqLevel", "name" } });
	cfg.layout.push_back(Entry {
		.modeIds  = { "uniqueJewel", "rareMagicJewel", "uniqueRingAmu", "rareRingAmu", "magicRingAmu" },
		.sortKeys = { "quality", "reqLevel", "name" },
	});
	cfg.layout.push_back(Entry {
		.modeIds    = { "uniqueGear", "set", "rareGear", "magicGear", "superiorGear", "normalGear" },
		.sortKeys   = { "-area", "handTier", "gearGroup", "quality", "reqLevel" },
		.gearGroups = allGear,
	});
	cfg.layout.push_back(Entry { .modeIds = { "unclassified" }, .anchor = Anchor::BottomLeft, .sortKeys = { "-area", "name" } });
	return cfg;
}

std::vector<std::string> ValidateConfig(const Config& cfg) {
	std::vector<std::string> errors;
	const std::string        p = "[" + cfg.name + "] ";

	if (cfg.width < 1 || cfg.width > kMaxGridSide || cfg.height < 1 || cfg.height > kMaxGridSide) {
		errors.push_back(p + "grid_width and grid_height must be 1-16 (got " + std::to_string(cfg.width) + " x " + std::to_string(cfg.height) + ")");
		return errors; // every rectangle check below depends on the grid size
	}

	for (const auto& r : cfg.lockedCells) {
		if (!RectInside(r, cfg.width, cfg.height)) errors.push_back(p + "locked_cells rectangle " + RectText(r) + " is empty or outside the grid");
	}

	std::unordered_set<std::string> keep;
	for (const auto& id : cfg.keepInPlace) {
		if (!IsKnownModeId(id)) errors.push_back(p + "keep_in_place has an unknown mode_id \"" + id + "\"");
		keep.insert(id);
	}

	if (cfg.layout.empty()) errors.push_back(p + "the layout list is empty -- add at least one [[...layout]] entry");

	std::unordered_set<std::string> seen;
	for (size_t i = 0; i < cfg.layout.size(); ++i) {
		const Entry&      e  = cfg.layout[i];
		const std::string ep = p + "layout entry " + std::to_string(i + 1) + ": ";
		if (e.modeIds.empty()) errors.push_back(ep + "set exactly one of mode_id or mode_ids");
		for (const auto& id : e.modeIds) {
			if (!IsKnownModeId(id)) {
				errors.push_back(ep + "unknown mode_id \"" + id + "\"");
				continue;
			}
			if (!seen.insert(id).second) errors.push_back(ep + "mode \"" + id + "\" already appears in an earlier entry");
			if (keep.count(id) != 0) errors.push_back(ep + "mode \"" + id + "\" is also listed in keep_in_place");
		}
		for (const auto& key : e.sortKeys) {
			const std::string bare = (!key.empty() && key[0] == '-') ? key.substr(1) : key;
			if (!IsKnownSortKey(bare)) errors.push_back(ep + "unknown sort key \"" + key + "\"");
		}
		if (e.hasZone) {
			if (!RectInside(e.zone, cfg.width, cfg.height)) {
				errors.push_back(ep + "zone " + RectText(e.zone) + " is empty or outside the grid");
				continue;
			}
			for (const auto& r : cfg.lockedCells) {
				if (RectsOverlap(e.zone, r)) errors.push_back(ep + "zone " + RectText(e.zone) + " overlaps locked_cells " + RectText(r));
			}
			for (size_t j = 0; j < i; ++j) {
				const Entry& other = cfg.layout[j];
				if (other.hasZone && RectInside(other.zone, cfg.width, cfg.height) && RectsOverlap(e.zone, other.zone)) {
					errors.push_back(ep + "zone " + RectText(e.zone) + " overlaps the zone of entry " + std::to_string(j + 1));
				}
			}
		}
	}
	return errors;
}

Sequence SequenceForLiveApply(const std::vector<ItemFacts>& items, const std::vector<PlannedMove>& moves, uint32_t width, uint32_t height) {
	Sequence out;
	const int32_t W = static_cast<int32_t>(width);
	const int32_t H = static_cast<int32_t>(height);

	struct Live {
		int32_t x, y, w, h;
	};
	std::unordered_map<uint64_t, Live> live; // every item's current cells
	std::vector<uint64_t>              owner(static_cast<size_t>(W) * H, 0);
	auto cellsDo = [&](const Live& l, auto&& fn) {
		for (int32_t yy = std::max(0, l.y); yy < std::min(H, l.y + l.h); ++yy)
			for (int32_t xx = std::max(0, l.x); xx < std::min(W, l.x + l.w); ++xx) fn(static_cast<size_t>(yy) * W + static_cast<size_t>(xx));
	};
	for (const auto& it : items) {
		Live l { it.x, it.y, static_cast<int32_t>(it.w), static_cast<int32_t>(it.h) };
		live[it.handle] = l;
		cellsDo(l, [&](size_t idx) { owner[idx] = it.handle; });
	}
	// `self` may overlap its own old cells: the game lifts an item out of the
	// grid before placing it (exactly what a player does when dropping an
	// item one cell over from where it was). Only OTHER items block.
	auto allFree = [&](int32_t x, int32_t y, int32_t w, int32_t h, uint64_t self) {
		if (x < 0 || y < 0 || x + w > W || y + h > H) return false;
		for (int32_t yy = y; yy < y + h; ++yy)
			for (int32_t xx = x; xx < x + w; ++xx) {
				const uint64_t o = owner[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)];
				if (o != 0 && o != self) return false;
			}
		return true;
	};
	auto apply = [&](uint64_t handle, int32_t x, int32_t y) {
		Live& l = live[handle];
		cellsDo(l, [&](size_t idx) { owner[idx] = 0; });
		l.x = x;
		l.y = y;
		cellsDo(l, [&](size_t idx) { owner[idx] = handle; });
		out.steps.push_back(PlannedMove { .handle = handle, .destTab = 0, .destX = x, .destY = y });
	};

	std::vector<PlannedMove> pending;
	for (const auto& mv : moves) {
		auto it = live.find(mv.handle);
		if (it == live.end()) {
			out.error = "move for an unknown item handle";
			return out;
		}
		if (it->second.x == mv.destX && it->second.y == mv.destY) continue;
		pending.push_back(mv);
	}

	// Cells some pending move still has to land on; parking avoids them when
	// it can so it doesn't create new blocks.
	auto reservedMask = [&]() {
		std::vector<bool> mask(static_cast<size_t>(W) * H, false);
		for (const auto& mv : pending) {
			const Live& l = live[mv.handle];
			for (int32_t yy = mv.destY; yy < mv.destY + l.h; ++yy)
				for (int32_t xx = mv.destX; xx < mv.destX + l.w; ++xx) mask[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)] = true;
		}
		return mask;
	};

	const size_t maxRounds = moves.size() * 4 + 64;
	size_t       rounds    = 0;
	while (!pending.empty()) {
		if (++rounds > maxRounds) {
			out.error = "no safe move order found (gave up after " + std::to_string(maxRounds) + " rounds)";
			return out;
		}

		// 1) Every move whose destination is completely empty right now.
		bool progress = false;
		for (size_t i = 0; i < pending.size();) {
			const PlannedMove mv = pending[i];
			const Live&       l  = live[mv.handle];
			if (allFree(mv.destX, mv.destY, l.w, l.h, mv.handle)) {
				apply(mv.handle, mv.destX, mv.destY);
				pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
				progress = true;
			} else {
				++i;
			}
		}
		if (progress) continue;

		// 2) Everything left is blocked. Pick one pending move M and clear its
		// destination: park every item standing on it (M's own item included,
		// when M overlaps its old cells) on free cells OUTSIDE M's destination,
		// preferring cells no other pending move needs, then move M. Every
		// round therefore finishes one move, so this always terminates.
		// Destinations of finished moves never overlap M's (the plan is
		// collision-free), so the blockers are always pending items. Try the
		// moves with the fewest / smallest blockers first.
		const auto reserved = reservedMask();
		struct Option {
			size_t                index;
			std::vector<uint64_t> blockers;
			int32_t               blockerArea;
		};
		std::vector<Option> options;
		for (size_t i = 0; i < pending.size(); ++i) {
			const PlannedMove& mv = pending[i];
			const Live&        l  = live[mv.handle];
			std::vector<uint64_t> blockers;
			for (int32_t yy = mv.destY; yy < mv.destY + l.h; ++yy)
				for (int32_t xx = mv.destX; xx < mv.destX + l.w; ++xx) {
					const uint64_t o = owner[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)];
					if (o != 0 && o != mv.handle && std::find(blockers.begin(), blockers.end(), o) == blockers.end()) blockers.push_back(o);
				}
			int32_t area = 0;
			for (uint64_t b : blockers) area += live[b].w * live[b].h;
			options.push_back(Option { i, std::move(blockers), area });
		}
		std::stable_sort(options.begin(), options.end(), [](const Option& a, const Option& b) {
			if (a.blockers.size() != b.blockers.size()) return a.blockers.size() < b.blockers.size();
			return a.blockerArea < b.blockerArea;
		});

		bool done = false;
		for (const Option& opt : options) {
			const PlannedMove mv = pending[opt.index];
			const Live        me = live[mv.handle];

			// Simulate on copies; commit only if every blocker finds a spot.
			auto                 ownerCopy = owner;
			auto                 liveCopy  = live;
			std::vector<PlannedMove> parks;
			bool                 allParked = true;
			std::vector<uint64_t> order = opt.blockers;
			std::stable_sort(order.begin(), order.end(), [&](uint64_t a, uint64_t b) {
				return live[a].w * live[a].h > live[b].w * live[b].h; // biggest first: hardest to fit
			});
			for (uint64_t b : order) {
				Live& lb = liveCopy[b];
				bool  placed = false;
				for (int pass = 0; pass < 2 && !placed; ++pass) { // pass 0: avoid other pending destinations
					for (int32_t y = 0; y + lb.h <= H && !placed; ++y) {
						for (int32_t x = 0; x + lb.w <= W && !placed; ++x) {
							bool ok = true;
							for (int32_t yy = y; yy < y + lb.h && ok; ++yy)
								for (int32_t xx = x; xx < x + lb.w && ok; ++xx) {
									const size_t idx = static_cast<size_t>(yy) * W + static_cast<size_t>(xx);
									const bool inDest = xx >= mv.destX && xx < mv.destX + me.w && yy >= mv.destY && yy < mv.destY + me.h;
									ok = (ownerCopy[idx] == 0 || ownerCopy[idx] == b) && !inDest && (pass == 1 || !reserved[idx]);
								}
							if (!ok) continue;
							for (int32_t yy = std::max(0, lb.y); yy < std::min(H, lb.y + lb.h); ++yy)
								for (int32_t xx = std::max(0, lb.x); xx < std::min(W, lb.x + lb.w); ++xx) ownerCopy[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)] = 0;
							lb.x = x;
							lb.y = y;
							for (int32_t yy = y; yy < y + lb.h; ++yy)
								for (int32_t xx = x; xx < x + lb.w; ++xx) ownerCopy[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)] = b;
							parks.push_back(PlannedMove { .handle = b, .destTab = 0, .destX = x, .destY = y });
							placed = true;
						}
					}
				}
				if (!placed) {
					allParked = false;
					break;
				}
			}
			if (!allParked) continue;

			for (const auto& pk : parks) apply(pk.handle, pk.destX, pk.destY);
			out.parkingMoves += static_cast<uint32_t>(parks.size());
			apply(mv.handle, mv.destX, mv.destY);
			pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(opt.index));
			done = true;
			break;
		}
		if (!done) {
			// 3) No destination can be cleared in one go. Reshape the free
			// space instead: move one pending item that sits on some pending
			// destination to any other free spot (first avoiding pending
			// destinations), smallest first, and try again next round.
			// maxRounds bounds this.
			std::vector<size_t> candidates;
			for (size_t i = 0; i < pending.size(); ++i) {
				const Live& l      = live[pending[i].handle];
				bool        blocks = false;
				cellsDo(l, [&](size_t idx) { blocks = blocks || reserved[idx]; });
				if (blocks) candidates.push_back(i);
			}
			std::stable_sort(candidates.begin(), candidates.end(), [&](size_t a, size_t b) {
				const Live& la = live[pending[a].handle];
				const Live& lb = live[pending[b].handle];
				return la.w * la.h < lb.w * lb.h;
			});
			// Rotate the start so repeated rounds don't keep shuffling the same item.
			if (!candidates.empty()) std::rotate(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(rounds % candidates.size()), candidates.end());
			for (int pass = 0; pass < 2 && !done; ++pass) {
				for (size_t ci : candidates) {
					const uint64_t handle = pending[ci].handle;
					const Live     l      = live[handle];
					for (int32_t y = 0; y + l.h <= H && !done; ++y) {
						for (int32_t x = 0; x + l.w <= W && !done; ++x) {
							if (x == l.x && y == l.y) continue;
							if (!allFree(x, y, l.w, l.h, handle)) continue;
							bool clash = false;
							for (int32_t yy = y; yy < y + l.h && !clash; ++yy)
								for (int32_t xx = x; xx < x + l.w && !clash; ++xx) {
									const bool ownOld = xx >= l.x && xx < l.x + l.w && yy >= l.y && yy < l.y + l.h;
									clash = !ownOld && (pass == 0 ? reserved[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)] : false);
								}
							if (clash) continue;
							// Must actually free a reserved cell, or it's no progress.
							bool frees = false;
							for (int32_t yy = l.y; yy < l.y + l.h && !frees; ++yy)
								for (int32_t xx = l.x; xx < l.x + l.w && !frees; ++xx)
									frees = reserved[static_cast<size_t>(yy) * W + static_cast<size_t>(xx)]
									     && !(xx >= x && xx < x + l.w && yy >= y && yy < y + l.h);
							if (!frees) continue;
							apply(handle, x, y);
							++out.parkingMoves;
							done = true;
						}
					}
					if (done) break;
				}
			}
		}
		if (!done) {
			out.error = "no safe move order: " + std::to_string(pending.size()) +
			            " item(s) block each other and there is not enough free space to move them aside (grid too full)";
			return out;
		}
	}

	out.ok = true;
	return out;
}

Plan PlanSort(const std::vector<ItemFacts>& items, const Config& cfg) {
	Plan plan;

	const auto errors = ValidateConfig(cfg);
	if (!errors.empty()) {
		plan.status = Status::ConfigError;
		plan.error  = errors.front();
		plan.log    = errors;
		return plan;
	}

	const int32_t W = static_cast<int32_t>(cfg.width);
	const int32_t H = static_cast<int32_t>(cfg.height);

	Occupancy base(cfg.width, cfg.height);
	for (const auto& r : cfg.lockedCells) base.Mark(r.x, r.y, r.w, r.h);
	const Occupancy lockedOnly = base;

	std::unordered_set<std::string> keep(cfg.keepInPlace.begin(), cfg.keepInPlace.end());
	const bool keepUnclaimed = keep.count("unclassified") != 0;

	int catchAllEntry = -1;
	for (size_t i = 0; i < cfg.layout.size() && catchAllEntry < 0; ++i) {
		for (const auto& id : cfg.layout[i].modeIds) {
			if (IsCatchAllMode(id)) {
				catchAllEntry = static_cast<int>(i);
				break;
			}
		}
	}

	Assignment assign;
	assign.perEntry.resize(cfg.layout.size());
	uint32_t outsideLogged = 0;
	uint64_t movableArea   = 0;

	for (const auto& it : items) {
		const int32_t iw = static_cast<int32_t>(it.w);
		const int32_t ih = static_cast<int32_t>(it.h);
		if (it.x < 0 || it.y < 0 || iw < 1 || ih < 1 || it.x + iw > W || it.y + ih > H) {
			++plan.itemsOutsideGrid;
			if (outsideLogged++ < 8) {
				char buf[200];
				std::snprintf(buf, sizeof(buf), "[%s] item code 0x%08X at (%d,%d) size %dx%d is outside the %ux%u grid -- left untouched", cfg.name.c_str(),
					it.code, it.x, it.y, iw, ih, cfg.width, cfg.height);
				plan.log.push_back(buf);
			}
			continue;
		}

		bool fixed = false;
		for (int32_t yy = it.y; yy < it.y + ih && !fixed; ++yy)
			for (int32_t xx = it.x; xx < it.x + iw && !fixed; ++xx) fixed = lockedOnly.At(xx, yy);
		if (!fixed) {
			for (const auto& id : cfg.keepInPlace) {
				if (GridModeMatches(id, it)) {
					fixed = true;
					break;
				}
			}
		}

		int target = -1;
		if (!fixed) {
			for (size_t i = 0; i < cfg.layout.size() && target < 0; ++i) {
				for (const auto& id : cfg.layout[i].modeIds) {
					if (GridModeMatches(id, it)) {
						target = static_cast<int>(i);
						break;
					}
				}
			}
			if (target < 0 && !keepUnclaimed) target = catchAllEntry;
		}

		if (target < 0) {
			assign.fixed.push_back(&it);
			base.Mark(it.x, it.y, iw, ih);
		} else {
			assign.perEntry[static_cast<size_t>(target)].push_back(&it);
			movableArea += static_cast<uint64_t>(iw) * static_cast<uint64_t>(ih);
		}
	}
	if (outsideLogged > 8) {
		plan.log.push_back("[" + cfg.name + "] ... and " + std::to_string(outsideLogged - 8) + " more item(s) outside the grid");
	}
	plan.itemsFixed = static_cast<uint32_t>(assign.fixed.size());

	uint64_t freeCells = 0;
	for (bool used : base.used) freeCells += used ? 0 : 1;
	if (movableArea > freeCells) {
		plan.status = Status::NoRoom;
		plan.error  = "[" + cfg.name + "] the items to arrange need " + std::to_string(movableArea) + " cells but only " + std::to_string(freeCells) +
		             " are free (locked cells and fixed items excluded)";
		return plan;
	}

	Attempt attempt = TryArrange(cfg, assign, base, cfg.categoryBreak);
	if (!attempt.ok && cfg.onOverflow == Overflow::Compact && cfg.categoryBreak == CategoryBreak::Band) {
		plan.log.push_back(attempt.error + " -- retrying with category_break = \"none\"");
		attempt                = TryArrange(cfg, assign, base, CategoryBreak::None);
		plan.compactRetryUsed  = true;
	}
	if (!attempt.ok) {
		plan.status = Status::NoRoom;
		plan.error  = attempt.error;
		return plan;
	}

	plan.log.insert(plan.log.end(), attempt.entryLog.begin(), attempt.entryLog.end());
	plan.itemsArranged = static_cast<uint32_t>(attempt.placed.size());
	for (const auto& [it, pos] : attempt.placed) {
		if (pos.first == it->x && pos.second == it->y) continue;
		plan.moves.push_back(PlannedMove { .handle = it->handle, .destTab = 0, .destX = pos.first, .destY = pos.second });
	}
	return plan;
}

}
