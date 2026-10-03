#include "sort_engine.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace ItemSorting {

namespace {

constexpr uint32_t kQualityInferior = 1;
constexpr uint32_t kQualityNormal   = 2;
constexpr uint32_t kQualitySuperior = 3;
constexpr uint32_t kQualityMagic    = 4;
constexpr uint32_t kQualitySet      = 5;
constexpr uint32_t kQualityRare     = 6;
constexpr uint32_t kQualityUnique   = 7;
constexpr uint32_t kQualityCrafted  = 8;

bool IsUniqueLike(const ItemFacts& it) { return it.quality == kQualityUnique || it.quality == kQualityCrafted; }
bool IsRare(const ItemFacts& it) { return it.quality == kQualityRare; }
bool IsMagic(const ItemFacts& it) { return it.quality == kQualityMagic; }
bool IsSuperior(const ItemFacts& it) { return it.quality == kQualitySuperior; }
bool IsNormalQuality(const ItemFacts& it) { return it.quality == kQualityNormal; }
bool IsSetQuality(const ItemFacts& it) { return it.quality == kQualitySet; }

// Crafted (q=8) sorts ahead of unique (q=7) inside each group -- mirrors
// _ssQualityRank. Every other quality ties under this key (it only exists to
// break the crafted/unique ambiguity within a single "uniqueGear"-like
// stream); the "quality" sort key name below resolves to this.
int64_t QualityRank(const ItemFacts& it) { return it.quality == kQualityCrafted ? 0 : 1; }

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

std::string TieBreakName(const ItemFacts& it) {
	return it.baseNameKey.empty() ? std::to_string(it.code) : it.baseNameKey;
}

// --- Generalized, config-driven sort keys -------------------------------
//
// Every category's sort order is now a named, ordered list of keys (a
// CategorySpec::sortKeys, parsed into ParsedSortKey below), resolved against
// one item at a time into a CompoundKey and compared field-by-field. This
// replaces what used to be a fixed 5-int/1-string SortKey with a hardcoded
// per-mode lambda: the field *count* and *meaning* are now whatever the
// config (or, absent config, the mode's own defaultSortKeys, chosen to
// reproduce the original hardcoded behavior exactly) says they are.

struct KeyField {
	bool        isText = false;
	int64_t     num    = 0;
	std::string text;
	bool        desc   = false; // true = "-" prefix on this key's name
};

struct CompoundKey {
	std::vector<KeyField> fields;
};

bool CompoundKeyLess(const CompoundKey& a, const CompoundKey& b) {
	const size_t n = std::min(a.fields.size(), b.fields.size());
	for (size_t i = 0; i < n; ++i) {
		const KeyField& fa = a.fields[i];
		const KeyField& fb = b.fields[i];
		if (fa.isText) {
			if (fa.text != fb.text) return fa.desc ? (fa.text > fb.text) : (fa.text < fb.text);
		} else {
			if (fa.num != fb.num) return fa.desc ? (fa.num > fb.num) : (fa.num < fb.num);
		}
	}
	return false;
}

struct ParsedSortKey {
	std::string name;
	bool        desc = false;
};

std::vector<ParsedSortKey> ParseSortKeys(const std::vector<std::string>& raw) {
	std::vector<ParsedSortKey> out;
	out.reserve(raw.size());
	for (const auto& s : raw) {
		if (s.empty()) continue;
		if (s[0] == '-')
			out.push_back({ s.substr(1), true });
		else
			out.push_back({ s, false });
	}
	return out;
}

// Default (natural) 23-slot gear order -- GearSlot's own declaration order,
// which matches SS_GEAR_GROUPS exactly (see the enum's comment in the
// header).
const std::vector<GearSlot>& DefaultGearOrder() {
	static const std::vector<GearSlot> order = {
		GearSlot::Helm,  GearSlot::Torso,    GearSlot::Belt,   GearSlot::Gloves, GearSlot::Boots, GearSlot::Shield,
		GearSlot::Axe,   GearSlot::Sword,    GearSlot::Knife,  GearSlot::ClubMace, GearSlot::Scepter, GearSlot::Wand,
		GearSlot::Staff, GearSlot::Orb,      GearSlot::Spear,  GearSlot::Polearm, GearSlot::Javelin, GearSlot::Bow,
		GearSlot::Arrows, GearSlot::Crossbow, GearSlot::Bolts, GearSlot::Claw,    GearSlot::OtherWeapon,
	};
	return order;
}

// The 23 short keys a config's gear_groups list uses, mapped to GearSlot.
// Matches the config file's own documented key list exactly.
GearSlot GearSlotFromConfigKey(const std::string& key) {
	static const std::unordered_map<std::string, GearSlot> map = {
		{ "helm", GearSlot::Helm },     { "tors", GearSlot::Torso },   { "belt", GearSlot::Belt },
		{ "glov", GearSlot::Gloves },   { "boot", GearSlot::Boots },   { "shld", GearSlot::Shield },
		{ "axe", GearSlot::Axe },       { "swor", GearSlot::Sword },   { "knif", GearSlot::Knife },
		{ "blun", GearSlot::ClubMace }, { "scep", GearSlot::Scepter }, { "wand", GearSlot::Wand },
		{ "staf", GearSlot::Staff },    { "orb", GearSlot::Orb },      { "spea", GearSlot::Spear },
		{ "pole", GearSlot::Polearm },  { "jave", GearSlot::Javelin }, { "bow", GearSlot::Bow },
		{ "bowq", GearSlot::Arrows },   { "xbow", GearSlot::Crossbow },{ "xboq", GearSlot::Bolts },
		{ "h2h", GearSlot::Claw },      { "weap", GearSlot::OtherWeapon },
	};
	auto found = map.find(key);
	return found == map.end() ? GearSlot::None : found->second;
}

// Builds this category's gear-group priority order (lower = sorts first):
// slots named in `gearGroupNames`, in the order given, come first; every
// slot left out keeps its default relative position, appended after in
// DefaultGearOrder's own order. An empty/all-unknown input is just the
// default order untouched.
std::unordered_map<GearSlot, int64_t> BuildGearGroupPriority(const std::vector<std::string>& gearGroupNames) {
	std::unordered_map<GearSlot, int64_t> priority;
	int64_t                               next = 0;
	for (const auto& name : gearGroupNames) {
		GearSlot slot = GearSlotFromConfigKey(name);
		if (slot == GearSlot::None) continue; // unknown key name -- ignore rather than fail the whole category
		if (priority.count(slot)) continue;   // duplicate -- first occurrence wins
		priority[slot] = next++;
	}
	for (GearSlot slot : DefaultGearOrder()) {
		if (!priority.count(slot)) priority[slot] = next++;
	}
	return priority;
}

// Resolves one named key (already stripped of any "-" prefix) against one
// item into a KeyField. `gearOrder` is this category's own resolved
// gear-group priority map (see BuildGearGroupPriority), used only by
// "gearGroup". Returns false for an unrecognized name so the caller can skip
// it silently -- a typo in a config's sort_keys degrades to "this key has no
// effect" rather than a hard failure.
bool ResolveKeyField(const std::string& name, const ItemFacts& it, const std::unordered_map<GearSlot, int64_t>& gearOrder, KeyField& out) {
	if (name == "reqLevel") {
		out.num = static_cast<int64_t>(it.sortLevel);
		return true;
	}
	if (name == "ilvl") {
		out.num = static_cast<int64_t>(it.itemLevel);
		return true;
	}
	if (name == "name" || name == "itemName") {
		out.isText = true;
		out.text   = TieBreakName(it);
		return true;
	}
	if (name == "quality") {
		out.num = QualityRank(it);
		return true;
	}
	if (name == "handTier") {
		out.num = HandTier(it);
		return true;
	}
	if (name == "gearGroup") {
		auto found = gearOrder.find(it.gearSlot);
		out.num    = found == gearOrder.end() ? static_cast<int64_t>(gearOrder.size()) : found->second;
		return true;
	}
	if (name == "setName") {
		out.isText = true;
		out.text   = it.setGroupName;
		return true;
	}
	if (name == "size") {
		out.num = static_cast<int64_t>(it.h);
		return true;
	}
	// Footprint keys, meaningful for any item. Mostly for the single-grid
	// containers (grid_sort.cpp), where placing big items first ("-area")
	// leaves fewer holes.
	if (name == "area") {
		out.num = static_cast<int64_t>(it.w) * static_cast<int64_t>(it.h);
		return true;
	}
	if (name == "height") {
		out.num = static_cast<int64_t>(it.h);
		return true;
	}
	// misc.txt keys (see ItemFacts::miscOrder/miscGrade). An item of another
	// kind resolves to 0 and simply ties.
	if (name == "runeNumber") {
		out.num = it.miscKind == MiscKind::Rune ? static_cast<int64_t>(it.miscOrder) * 2 + (it.miscGrade != 0 ? 1 : 0) : 0;
		return true;
	}
	if (name == "gemType" || name == "potionType") {
		const MiscKind want = name == "gemType" ? MiscKind::Gem : MiscKind::Potion;
		out.num             = it.miscKind == want ? static_cast<int64_t>(it.miscOrder) : 0;
		return true;
	}
	if (name == "gemGrade" || name == "potionGrade") {
		const MiscKind want = name == "gemGrade" ? MiscKind::Gem : MiscKind::Potion;
		out.num             = it.miscKind == want ? static_cast<int64_t>(it.miscGrade) : 0;
		return true;
	}
	return false;
}

const char* const kKnownSortKeys[] = {
	"reqLevel", "ilvl", "name", "itemName", "quality", "handTier", "gearGroup", "setName", "size",
	"area", "height", "runeNumber", "gemType", "gemGrade", "potionType", "potionGrade",
};

CompoundKey BuildSortKey(
	const ItemFacts& it, const std::vector<ParsedSortKey>& keys, const std::unordered_map<GearSlot, int64_t>& gearOrder) {
	CompoundKey k;
	k.fields.reserve(keys.size());
	for (const auto& pk : keys) {
		KeyField f;
		f.desc = pk.desc;
		if (!ResolveKeyField(pk.name, it, gearOrder, f)) continue;
		k.fields.push_back(f);
	}
	return k;
}

enum class FillDir { Row, ColLeft, ColRight };

struct Stream {
	std::function<bool(const ItemFacts&)> match;
	FillDir                                dir = FillDir::Row;
};

struct Mode {
	std::string              id;
	bool                     atomicSets = false;
	// Dynamic catch-all: this mode's real match predicate isn't its stream's
	// (which is a placeholder, always false) -- it is "not matched by any
	// OTHER mode present in the current layout", computed fresh per
	// RunAutoSort call (see `claimedByOthers`/`streamMatches` there) since it
	// depends on which categories the caller's config actually lists.
	bool                     catchAll = false;
	// Used when a CategorySpec's own sortKeys is empty -- chosen, per mode, to
	// reproduce this plugin's original hardcoded ordering exactly.
	std::vector<std::string> defaultSortKeys;
	std::vector<Stream>      streams;
};

// Resolves the sort_keys this category actually sorts by: its own config
// override if it set one, else its mode's built-in default.
std::vector<ParsedSortKey> EffectiveSortKeys(const CategorySpec& spec, const Mode& mode) {
	return ParseSortKeys(spec.sortKeys.empty() ? mode.defaultSortKeys : spec.sortKeys);
}

// Mirrors SS_SORT_MODES / _ssRingAmuletModes. Ring fills from the left
// column, amulet from the right, so the two never interleave.
Stream RingAmuStream(bool isRing, std::function<bool(const ItemFacts&)> qualityMatch) {
	Stream s;
	s.dir = isRing ? FillDir::ColLeft : FillDir::ColRight;
	NonGearKind want = isRing ? NonGearKind::Ring : NonGearKind::Amulet;
	s.match = [qualityMatch, want](const ItemFacts& it) { return qualityMatch(it) && it.nonGearKind == want; };
	return s;
}

const std::vector<Mode>& BuiltinModes() {
	static const std::vector<Mode> modes = [] {
		std::vector<Mode> m;

		m.push_back(Mode {
			.id              = "set",
			.atomicSets      = true,
			.defaultSortKeys = { "setName", "reqLevel", "-ilvl", "itemName" },
			.streams         = { Stream {
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
			   } },
		});

		m.push_back(Mode {
			.id              = "uniqueGear",
			.defaultSortKeys = { "handTier", "gearGroup", "quality", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.isGear; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "rareGear",
			.defaultSortKeys = { "handTier", "gearGroup", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsRare(it) && it.isGear; },
				   .dir   = FillDir::Row,
			   } },
		});

		// NEW -- magic/superior/normal quality-tier gear. Before these existed,
		// a magic/superior/normal-quality helm/weapon/etc had no dedicated
		// category and fell all the way through to "unclassified" below; now
		// each quality tier gets its own tab(s) just like unique/rare already
		// had.
		m.push_back(Mode {
			.id              = "magicGear",
			.defaultSortKeys = { "handTier", "gearGroup", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsMagic(it) && it.isGear; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "superiorGear",
			.defaultSortKeys = { "handTier", "gearGroup", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsSuperior(it) && it.isGear; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "normalGear",
			.defaultSortKeys = { "handTier", "gearGroup", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsNormalQuality(it) && it.isGear; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "uniqueCharm",
			.defaultSortKeys = { "-size", "quality", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.nonGearKind == NonGearKind::Charm; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "magicCharm",
			.defaultSortKeys = { "-size", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsMagic(it) && it.nonGearKind == NonGearKind::Charm; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "uniqueJewel",
			.defaultSortKeys = { "quality", "reqLevel", "-ilvl", "name" },
			.streams         = { Stream {
				   .match = [](const ItemFacts& it) { return IsUniqueLike(it) && it.nonGearKind == NonGearKind::Jewel; },
				   .dir   = FillDir::Row,
			   } },
		});

		m.push_back(Mode {
			.id              = "rareMagicJewel",
			.defaultSortKeys = { "reqLevel", "-ilvl", "name" },
			.streams =
				{
					Stream {
						.match = [](const ItemFacts& it) { return IsRare(it) && it.nonGearKind == NonGearKind::Jewel; },
						.dir   = FillDir::ColLeft,
					},
					Stream {
						.match = [](const ItemFacts& it) { return IsMagic(it) && it.nonGearKind == NonGearKind::Jewel; },
						.dir   = FillDir::ColRight,
					},
				},
		});

		m.push_back(Mode { .id              = "uniqueRingAmu",
			.defaultSortKeys                = { "quality", "reqLevel", "-ilvl", "name" },
			.streams                        = { RingAmuStream(true, IsUniqueLike), RingAmuStream(false, IsUniqueLike) } });
		m.push_back(Mode { .id              = "rareRingAmu",
			.defaultSortKeys                = { "reqLevel", "-ilvl", "name" },
			.streams                        = { RingAmuStream(true, IsRare), RingAmuStream(false, IsRare) } });
		m.push_back(Mode { .id              = "magicRingAmu",
			.defaultSortKeys                = { "reqLevel", "-ilvl", "name" },
			.streams                        = { RingAmuStream(true, IsMagic), RingAmuStream(false, IsMagic) } });

		// misc.txt categories (see MiscKind). Mainly for the single-grid
		// containers, but valid in the shared-stash layout too.
		auto miscMode = [](const char* id, MiscKind kind, std::vector<std::string> keys) {
			return Mode {
				.id              = id,
				.defaultSortKeys = std::move(keys),
				.streams         = { Stream { .match = [kind](const ItemFacts& it) { return it.miscKind == kind; }, .dir = FillDir::Row } },
			};
		};
		m.push_back(miscMode("rune", MiscKind::Rune, { "runeNumber" }));
		m.push_back(miscMode("gem", MiscKind::Gem, { "gemType", "-gemGrade" }));
		m.push_back(miscMode("potion", MiscKind::Potion, { "potionType", "potionGrade" }));
		m.push_back(miscMode("tool", MiscKind::Tool, { "-area", "name" }));
		m.push_back(miscMode("quest", MiscKind::Quest, { "-area", "name" }));

		// NEW -- the catch-all. Its stream's match() is never actually called
		// (RunAutoSort special-cases catchAll: see `streamMatches` there,
		// which uses "not claimed by any other mode in THIS layout" instead)
		// -- it exists only so this mode still has exactly one Row-direction
		// stream to iterate, keeping it structurally identical to every other
		// non-atomic mode.
		m.push_back(Mode {
			.id              = "unclassified",
			.catchAll        = true,
			.defaultSortKeys = { "quality", "reqLevel", "name" },
			.streams         = { Stream { .match = [](const ItemFacts&) { return false; }, .dir = FillDir::Row } },
		});

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
	uint32_t          freeCount = 0; // cells still false; lets a placement skip a tab too full to hold the item at all

	explicit Occupancy(uint32_t gw = 0, uint32_t gh = 0) : w(gw), h(gh), cells(static_cast<size_t>(gw) * gh, false), freeCount(gw * gh) {}

	bool Free(int32_t x, int32_t y, uint32_t iw, uint32_t ih) const {
		if (x < 0 || y < 0) return false;
		if (static_cast<uint32_t>(x) + iw > w || static_cast<uint32_t>(y) + ih > h) return false;
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx)
				if (cells[static_cast<size_t>(y + dy) * w + (x + dx)]) return false;
		return true;
	}

	void Mark(int32_t x, int32_t y, uint32_t iw, uint32_t ih) {
		for (uint32_t dy = 0; dy < ih; ++dy) {
			for (uint32_t dx = 0; dx < iw; ++dx) {
				const size_t i = static_cast<size_t>(y + dy) * w + (x + dx);
				if (!cells[i]) {
					cells[i] = true;
					--freeCount;
				}
			}
		}
	}
};

// Like Occupancy, but each occupied cell remembers WHICH handle sits there --
// used by SequenceMovesForLiveApply to simulate live occupancy as moves are
// walked in dependency order (Occupancy's plain bool is enough for
// DetectCollisions, which only ever asks "is this free", never "who is
// this").
struct OwnerGrid {
	static constexpr uint64_t kNoOwner = (std::numeric_limits<uint64_t>::max)();

	uint32_t              w = 0, h = 0;
	std::vector<uint64_t> owner; // row-major, size w*h, kNoOwner = empty

	explicit OwnerGrid(uint32_t gw = 0, uint32_t gh = 0) : w(gw), h(gh), owner(static_cast<size_t>(gw) * gh, kNoOwner) {}

	bool InBounds(int32_t x, int32_t y, uint32_t iw, uint32_t ih) const {
		return x >= 0 && y >= 0 && static_cast<uint32_t>(x) + iw <= w && static_cast<uint32_t>(y) + ih <= h;
	}

	bool AllFree(int32_t x, int32_t y, uint32_t iw, uint32_t ih) const {
		if (!InBounds(x, y, iw, ih)) return false;
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx)
				if (owner[static_cast<size_t>(y + dy) * w + (x + dx)] != kNoOwner) return false;
		return true;
	}

	// Collects the distinct non-empty owners overlapping the block (out-of-
	// range cells are silently skipped rather than treated as occupied --
	// callers only use this on destinations already validated elsewhere).
	void CollectOwners(int32_t x, int32_t y, uint32_t iw, uint32_t ih, std::vector<uint64_t>& out) const {
		for (uint32_t dy = 0; dy < ih; ++dy) {
			for (uint32_t dx = 0; dx < iw; ++dx) {
				const int32_t xx = x + static_cast<int32_t>(dx);
				const int32_t yy = y + static_cast<int32_t>(dy);
				if (xx < 0 || yy < 0 || static_cast<uint32_t>(xx) >= w || static_cast<uint32_t>(yy) >= h) continue;
				const uint64_t o = owner[static_cast<size_t>(yy) * w + static_cast<uint32_t>(xx)];
				if (o != kNoOwner) out.push_back(o);
			}
		}
	}

	void Mark(int32_t x, int32_t y, uint32_t iw, uint32_t ih, uint64_t who) {
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx) owner[static_cast<size_t>(y + dy) * w + (x + dx)] = who;
	}

	void Clear(int32_t x, int32_t y, uint32_t iw, uint32_t ih) {
		if (!InBounds(x, y, iw, ih)) return;
		for (uint32_t dy = 0; dy < ih; ++dy)
			for (uint32_t dx = 0; dx < iw; ++dx) owner[static_cast<size_t>(y + dy) * w + (x + dx)] = kNoOwner;
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
		{ "uniqueGear", 5, true, EmptyPageRule::Never() },
		{ "uniqueRingAmu", 1, true, EmptyPageRule::Never() },
		{ "rareRingAmu", 1, true, EmptyPageRule::Never() },
		{ "magicRingAmu", 1, true, EmptyPageRule::Never() },
		{ "uniqueCharm", 1, true, EmptyPageRule::Never() },
		{ "magicCharm", 2, true, EmptyPageRule::Never() },
		{ "uniqueJewel", 1, true, EmptyPageRule::Never() },
		{ "rareMagicJewel", 1, true, EmptyPageRule::Never() },
		{ "set", 6, true, EmptyPageRule::AtPercent(0) }, // "always"
		{ "rareGear", 1, true, EmptyPageRule::Never() },
		{ "magicGear", 1, true, EmptyPageRule::Never() },
		{ "superiorGear", 1, true, EmptyPageRule::Never() },
		{ "normalGear", 1, true, EmptyPageRule::Never() },
		{ "unclassified", 1, true, EmptyPageRule::Never() },
	};
}

bool IsKnownModeId(const std::string& id) { return FindMode(id) != nullptr; }

bool ModeMatchesItem(const std::string& id, const ItemFacts& it) {
	const Mode* mode = FindMode(id);
	if (mode == nullptr || mode->catchAll) return false;
	for (const auto& stream : mode->streams) {
		if (stream.match(it)) return true;
	}
	return false;
}

bool IsCatchAllMode(const std::string& id) {
	const Mode* mode = FindMode(id);
	return mode != nullptr && mode->catchAll;
}

bool IsGearModeId(const std::string& id) {
	return id == "set" || id == "uniqueGear" || id == "rareGear" || id == "magicGear" || id == "superiorGear" || id == "normalGear";
}

bool IsKnownSortKey(const std::string& name) {
	for (const char* key : kKnownSortKeys) {
		if (name == key) return true;
	}
	return false;
}

std::vector<std::string> DefaultSortKeysForMode(const std::string& id) {
	const Mode* mode = FindMode(id);
	return mode == nullptr ? std::vector<std::string> {} : mode->defaultSortKeys;
}

std::function<bool(const ItemFacts&, const ItemFacts&)> MakeItemOrder(
	const std::vector<std::string>& sortKeys, const std::vector<std::string>& gearGroups) {
	auto keys      = ParseSortKeys(sortKeys);
	auto gearOrder = BuildGearGroupPriority(gearGroups);
	return [keys = std::move(keys), gearOrder = std::move(gearOrder)](const ItemFacts& a, const ItemFacts& b) {
		return CompoundKeyLess(BuildSortKey(a, keys, gearOrder), BuildSortKey(b, keys, gearOrder));
	};
}

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
	// "unclassified" (Mode::catchAll) has no static match predicate of its
	// own -- it matches whatever every OTHER mode actually present in THIS
	// layout does not. Compute that "claimed" set first, from every non-
	// catchAll mode's real streams, so streamMatches below can answer for a
	// catchAll mode too. If a layout removes a category entirely (e.g. no
	// "rareGear" entry), rareGear's items are simply never added to
	// claimedByOthers, so unclassified's "not claimed" test naturally scoops
	// them up -- no special-casing needed beyond this.
	std::unordered_set<uint64_t> claimedByOthers;
	claimedByOthers.reserve(items.size());
	for (const auto& spec : layout) {
		const Mode* mode = FindMode(spec.id);
		if (mode == nullptr || mode->catchAll) continue;
		for (const auto& stream : mode->streams)
			for (const auto& it : items)
				if (stream.match(it)) claimedByOthers.insert(it.handle);
	}
	auto streamMatches = [&claimedByOthers](const Mode* mode, const Stream& stream, const ItemFacts& it) -> bool {
		if (mode->catchAll) return claimedByOthers.find(it.handle) == claimedByOthers.end();
		return stream.match(it);
	};

	std::unordered_set<uint64_t> willBeMoved;
	willBeMoved.reserve(items.size());
	for (const auto& spec : layout) {
		const Mode* mode = FindMode(spec.id);
		if (mode == nullptr) continue; // reported as a hard error once this step is actually reached below
		for (const auto& stream : mode->streams)
			for (const auto& it : items)
				if (streamMatches(mode, stream, it)) willBeMoved.insert(it.handle);
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

		// Resolved once per category (config-driven, same for every stream and
		// every borrow retry within this category -- see EffectiveSortKeys /
		// BuildGearGroupPriority).
		const std::vector<ParsedSortKey>            sortKeys  = EffectiveSortKeys(spec, *mode);
		const std::unordered_map<GearSlot, int64_t> gearOrder = BuildGearGroupPriority(spec.gearGroups);

		// Each stream's items in placement order, plus the cells the whole
		// category needs. Neither depends on which tabs are being tried, so
		// both are built once here instead of on every borrow retry below
		// (a big category like rareGear can retry dozens of times). Sort keys
		// are built once per item rather than inside the comparator.
		std::vector<std::vector<const ItemFacts*>> sortedStreams;
		uint64_t                                   categoryCells = 0;
		if (!mode->atomicSets) {
			for (const auto& stream : mode->streams) {
				std::vector<std::pair<CompoundKey, const ItemFacts*>> keyed;
				for (const auto& it : items)
					if (streamMatches(mode, stream, it)) keyed.emplace_back(BuildSortKey(it, sortKeys, gearOrder), &it);
				std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return CompoundKeyLess(a.first, b.first); });
				std::vector<const ItemFacts*> sorted;
				sorted.reserve(keyed.size());
				for (const auto& entry : keyed) {
					sorted.push_back(entry.second);
					categoryCells += static_cast<uint64_t>(entry.second->w) * entry.second->h;
				}
				sortedStreams.push_back(std::move(sorted));
			}
		} else {
			for (const auto& it : items)
				if (mode->streams[0].match(it) && !it.setGroupName.empty()) categoryCells += static_cast<uint64_t>(it.w) * it.h;
		}

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
				// Sizes that already failed to fit in a dest tab during this
				// attempt. Occupancy only grows within an attempt, so an item at
				// least that wide AND tall cannot fit there either -- skipping
				// the tab gives exactly the same placement without rescanning
				// every cell of every nearly-full tab for every item.
				std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> failedSizes;
				for (size_t streamIndex = 0; streamIndex < mode->streams.size(); ++streamIndex) {
					const auto&                          stream  = mode->streams[streamIndex];
					const std::vector<const ItemFacts*>& matched = sortedStreams[streamIndex];
					if (matched.empty()) continue;
					result.matchedAnything = true;

					uint64_t totalCells = 0;
					for (const ItemFacts* it : matched) totalCells += static_cast<uint64_t>(it->w) * it->h;
					const uint64_t capacityPerTab   = static_cast<uint64_t>(gridWidth) * gridHeight;
					const uint64_t estimatedMinTabs = capacityPerTab == 0 ? 0 : (totalCells + capacityPerTab - 1) / capacityPerTab;

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
							Occupancy& occ = occByTab.at(dest);
							if (occ.freeCount < it->w * it->h) continue;
							auto& failed = failedSizes[dest];
							bool  hopeless = false;
							for (const auto& [fw, fh] : failed) {
								if (it->w >= fw && it->h >= fh) {
									hopeless = true;
									break;
								}
							}
							if (hopeless) continue;
							placed = PlaceBestFit(occ, order, it->w, it->h);
							if (placed) {
								chosenTab = dest;
								break;
							}
							failed.emplace_back(it->w, it->h);
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
				// Atomic-set placement: group by setGroupName, each group
				// placed whole into one dest tab, or split across two
				// consecutive dest tabs if it does not fit in one (mirrors
				// _ssPlanSort's Phase 1 / Phase 2). Group order and each
				// group's own item order are both config-driven: "setName" in
				// sortKeys picks the group order/direction (default
				// ascending, same as a bare std::map always gave before this
				// was configurable); every other key in sortKeys, in order,
				// sorts the items within one group before packing.
				bool                        setNameDesc = false;
				bool                        sawSetName  = false;
				std::vector<ParsedSortKey> intraGroupKeys;
				for (const auto& pk : sortKeys) {
					if (!sawSetName && pk.name == "setName") {
						setNameDesc = pk.desc;
						sawSetName  = true;
						continue;
					}
					intraGroupKeys.push_back(pk);
				}

				std::map<std::string, std::vector<const ItemFacts*>> groups; // std::map keeps ascending-by-name iteration order
				for (const auto& it : items) {
					if (!mode->streams[0].match(it)) continue;
					// match() itself now requires a non-empty setGroupName (see
					// the "set" mode's own comment) -- this is just a defensive
					// belt-and-suspenders check kept in case that ever changes.
					if (it.setGroupName.empty()) continue;
					groups[it.setGroupName].push_back(&it);
				}
				for (auto& [groupName, groupItems] : groups) {
					std::stable_sort(groupItems.begin(), groupItems.end(), [&](const ItemFacts* a, const ItemFacts* b) {
						return CompoundKeyLess(BuildSortKey(*a, intraGroupKeys, gearOrder), BuildSortKey(*b, intraGroupKeys, gearOrder));
					});
				}

				std::vector<std::map<std::string, std::vector<const ItemFacts*>>::iterator> groupOrder;
				groupOrder.reserve(groups.size());
				for (auto it = groups.begin(); it != groups.end(); ++it) groupOrder.push_back(it);
				if (setNameDesc) std::reverse(groupOrder.begin(), groupOrder.end());

				for (auto& groupIt : groupOrder) {
					const std::string&              groupName  = groupIt->first;
					std::vector<const ItemFacts*>&  groupItems = groupIt->second;
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

		// No set of tabs with less total room than the category's own cells
		// can ever fit it, so start borrowing at that lower bound instead of
		// at zero -- the first attempt that fits is the same one as before,
		// minus the retries that were bound to fail.
		const uint64_t capacityPerTab = static_cast<uint64_t>(gridWidth) * gridHeight;
		const uint64_t minTabsNeeded  = capacityPerTab == 0 ? 0 : (categoryCells + capacityPerTab - 1) / capacityPerTab;
		const uint32_t borrowRoom     = tabCount > cursor + reserved ? tabCount - cursor - reserved : 0u;
		uint32_t       borrowed       = minTabsNeeded > reserved
		                                    ? static_cast<uint32_t>(std::min<uint64_t>(minTabsNeeded - reserved, borrowRoom))
		                                    : 0u;
		Attempt        attempt;
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
				line += "pages [" + PageText(cursor) + ".." + PageText(cursor + span - 1) + "] (" + std::to_string(reserved)
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
			out.push_back("move for unknown handle " + std::to_string(mv.handle) + " -> page " + PageText(mv.destTab));
			continue;
		}
		const ItemFacts& fact = *found->second;
		Occupancy&        grid = tabGrid(mv.destTab);
		if (!grid.Free(mv.destX, mv.destY, fact.w, fact.h)) {
			char codeHex[16];
			std::snprintf(codeHex, sizeof(codeHex), "%08X", fact.code);
			out.push_back(
				std::string("COLLISION: item code 0x") + codeHex + " (" + std::to_string(fact.w) + "x" + std::to_string(fact.h)
				+ ") -> page " + PageText(mv.destTab) + " @ " + CellText(mv.destX, mv.destY) + " is already occupied");
			if (out.size() >= 20) break;
			continue;
		}
		grid.Mark(mv.destX, mv.destY, fact.w, fact.h);
	}

	return out;
}

namespace {

// First-fit search for a cell guaranteed free for the whole rest of this
// sequencing run: not occupied by anything live right now (per `live`) and
// not any move's own planned destination (per `reserved`) -- see
// SequenceMovesForLiveApply's header comment. Scans tabs in ascending order,
// row-major within a tab.
//
// Tries [0, preferredMaxTab) first, then -- only if that range has nothing
// free -- falls back to [preferredMaxTab, tabCount). A live report traced a
// cluster of `Result::Unsupported` move rejections to exactly the "falls
// back" case: a busy stash can genuinely have no free cell left anywhere in
// the tabs the plan's own categories use, so a plain single-range scan
// starting at 0 ends up spilling into tabs the plan never places a single
// real item into (bare padding `tabCount` leaves above the real stash size)
// -- see SequencedMoves::isScratchMove's comment for why landing there
// specifically causes live rejections. Preferring the plan's own known-safe
// range first avoids that whenever there's room for it, which the common
// case (a stash that isn't nearly 100% full) always has.
std::optional<std::tuple<uint32_t, int32_t, int32_t>> FindScratchCell(
	std::unordered_map<uint32_t, OwnerGrid>& live,
	std::unordered_map<uint32_t, Occupancy>& reserved,
	uint32_t                                 tabCount,
	uint32_t                                 gridWidth,
	uint32_t                                 gridHeight,
	uint32_t                                 iw,
	uint32_t                                 ih,
	uint32_t                                 preferredMaxTab) {
	if (iw == 0 || ih == 0 || iw > gridWidth || ih > gridHeight) return std::nullopt;

	auto scanRange = [&](uint32_t fromTab, uint32_t toTab) -> std::optional<std::tuple<uint32_t, int32_t, int32_t>> {
		for (uint32_t tab = fromTab; tab < toTab; ++tab) {
			auto liveIt = live.find(tab);
			auto resIt  = reserved.find(tab);
			for (uint32_t y = 0; y + ih <= gridHeight; ++y) {
				for (uint32_t x = 0; x + iw <= gridWidth; ++x) {
					if (liveIt != live.end() && !liveIt->second.AllFree(static_cast<int32_t>(x), static_cast<int32_t>(y), iw, ih)) continue;
					if (resIt != reserved.end() && !resIt->second.Free(static_cast<int32_t>(x), static_cast<int32_t>(y), iw, ih)) continue;
					return std::make_tuple(tab, static_cast<int32_t>(x), static_cast<int32_t>(y));
				}
			}
		}
		return std::nullopt;
	};

	const uint32_t firstPassEnd = (std::min)(preferredMaxTab, tabCount);
	if (auto found = scanRange(0, firstPassEnd)) return found;
	return scanRange(firstPassEnd, tabCount);
}

} // namespace

SequencedMoves SequenceMovesForLiveApply(
	const std::vector<ItemFacts>& items, const std::vector<PlannedMove>& moves, uint32_t tabCount, uint32_t gridWidth,
	uint32_t gridHeight, uint32_t preferredMaxTab) {
	SequencedMoves result;

	std::unordered_map<uint64_t, const ItemFacts*> byHandle;
	byHandle.reserve(items.size());
	for (const auto& it : items) byHandle.emplace(it.handle, &it);

	// Collapse to one net move per handle -- only the LAST planned move for a
	// handle is where it actually ends up (same reasoning as DetectCollisions
	// above); the live game only ever needs to see it move once, straight
	// from wherever it currently, physically sits.
	std::vector<uint64_t>                     order; // handles, in first-seen order
	std::unordered_map<uint64_t, PlannedMove> finalMoveByHandle;
	for (const auto& mv : moves) {
		if (finalMoveByHandle.find(mv.handle) == finalMoveByHandle.end()) order.push_back(mv.handle);
		finalMoveByHandle[mv.handle] = mv;
	}

	struct Node {
		PlannedMove            mv;
		uint32_t               w = 1, h = 1;
		uint32_t               curTab = kUnknownTab; // where this handle actually sits right now, live
		int32_t                curX = 0, curY = 0;
		int                    inDegree = 0;
		std::vector<uint64_t>  dependents; // handles whose move is blocked on this one moving out first
		bool                   done   = false;
		bool                   usable = true;  // false when live facts for it are missing
		bool                   broken = false; // true once this node's cycle has been scratch-broken (never re-break it)
		PlannedMove            scratchMove; // set only when this node's cycle got broken (see the stall branch below)
	};
	std::unordered_map<uint64_t, Node> nodes;
	nodes.reserve(order.size());
	for (uint64_t h : order) {
		Node n;
		n.mv = finalMoveByHandle[h];
		if (auto it = byHandle.find(h); it != byHandle.end()) {
			n.w      = it->second->w;
			n.h      = it->second->h;
			n.curTab = it->second->sharedStashPage;
			n.curX   = it->second->x;
			n.curY   = it->second->y;
		} else {
			n.usable = false; // DetectCollisions already flags unknown handles elsewhere
		}
		nodes.emplace(h, std::move(n));
	}

	// Reserve every move's own final destination up front (immutable for the
	// whole run) so a scratch-cell search never hands out a cell something
	// else is headed for later.
	std::unordered_map<uint32_t, Occupancy> reservedByTab;
	auto                                    reservedGrid = [&](uint32_t tab) -> Occupancy& {
        auto found = reservedByTab.find(tab);
        if (found != reservedByTab.end()) return found->second;
        return reservedByTab.emplace(tab, Occupancy(gridWidth, gridHeight)).first->second;
	};
	for (uint64_t h : order) {
		const Node& n = nodes[h];
		if (!n.usable) continue;
		reservedGrid(n.mv.destTab).Mark(n.mv.destX, n.mv.destY, n.w, n.h);
	}

	// Seed live occupancy from the CURRENT, real state (every item, whether
	// or not this plan touches it -- an item this plan leaves alone can
	// still be sitting exactly where a moved item is headed).
	std::unordered_map<uint32_t, OwnerGrid> liveByTab;
	auto                                    liveGrid = [&](uint32_t tab) -> OwnerGrid& {
        auto found = liveByTab.find(tab);
        if (found != liveByTab.end()) return found->second;
        return liveByTab.emplace(tab, OwnerGrid(gridWidth, gridHeight)).first->second;
	};
	for (const auto& it : items) {
		if (it.sharedStashPage == kUnknownTab) continue;
		liveGrid(it.sharedStashPage).Mark(it.x, it.y, it.w, it.h, it.handle);
	}

	// Build dependency edges: move(h) depends on move(occ) whenever occ is
	// currently sitting somewhere inside h's planned destination footprint
	// (a footprint bigger than the source item's own can straddle more than
	// one current occupant).
	for (uint64_t h : order) {
		Node& n = nodes[h];
		if (!n.usable) continue;
		std::vector<uint64_t> occupants;
		liveGrid(n.mv.destTab).CollectOwners(n.mv.destX, n.mv.destY, n.w, n.h, occupants);
		std::sort(occupants.begin(), occupants.end());
		occupants.erase(std::unique(occupants.begin(), occupants.end()), occupants.end());
		for (uint64_t occ : occupants) {
			if (occ == h) continue; // already sitting there -- no-op, no dependency
			auto occNode = nodes.find(occ);
			if (occNode == nodes.end()) {
				result.diagnostics.push_back(
					"move for handle " + std::to_string(h) + " -> page " + PageText(n.mv.destTab) + " "
					+ CellText(n.mv.destX, n.mv.destY) + " is currently occupied by handle "
					+ std::to_string(occ) + ", which this plan never moves -- this move may still be rejected live");
				continue;
			}
			occNode->second.dependents.push_back(h);
			n.inDegree++;
		}
	}

	// Kahn's algorithm, processed one full BFS wave ("layer") at a time
	// rather than a plain FIFO: everything in `currentLayer` has its
	// dependencies already satisfied by a STRICTLY EARLIER layer, never by
	// another move in the same layer, so the whole layer is safe to hand to
	// the live game as one unit regardless of how it batches/orders calls
	// within it (see SequencedMoves::layerSizes in the header). A stall
	// (`currentLayer` empty but nodes remain) means everything left forms
	// one or more dependency cycles; break the first unresolved one by
	// parking a member in a scratch cell -- see the header comment on
	// SequenceMovesForLiveApply.
	std::vector<uint64_t> currentLayer;
	for (uint64_t h : order) {
		if (!nodes[h].usable) { nodes[h].done = true; continue; }
		if (nodes[h].inDegree == 0) currentLayer.push_back(h);
	}

	size_t processed = 0;
	for (uint64_t h : order) if (nodes[h].done) ++processed;
	int cyclesBroken = 0;

	while (processed < order.size()) {
		if (currentLayer.empty()) {
			// Stalled: everything left forms one or more dependency cycles.
			// A full stash sort can easily contain many DISJOINT cycles (a
			// bunch of unrelated pairwise swaps, say), and they don't
			// depend on each other at all -- so break ALL of them in this
			// one pass, as a single scratch layer, instead of walking the
			// order array over and over to peel off one cycle per stall.
			// Breaking one member of a cycle can immediately zero out its
			// own cycle-mate's inDegree (see the loop body below), so a
			// node is only picked if it STILL has inDegree > 0 by the time
			// this pass reaches it -- that keeps this to exactly one break
			// per disjoint cycle, never two.
			const size_t          processedBefore = processed;
			std::vector<uint64_t> scratchLayer;
			for (uint64_t h : order) {
				Node& n = nodes[h];
				if (n.done || n.broken || n.inDegree == 0) continue;

				auto scratch = FindScratchCell(liveByTab, reservedByTab, tabCount, gridWidth, gridHeight, n.w, n.h, preferredMaxTab);
				if (!scratch) {
					// No free cell exists to break this cycle with (only
					// expected in an almost entirely full stash, or a
					// pathological pile-up of many simultaneous cycles all
					// competing for the same scarce large-item-sized gaps).
					// Emit its ORIGINAL move anyway rather than silently
					// dropping it: its real destination is still occupied by
					// its cycle-mate, so the live game will very likely
					// reject it, but ApplyMoves' own per-op logging will
					// then say exactly that (with the real result code),
					// which is far more useful than the item just quietly
					// never moving with no trace of why.
					result.diagnostics.push_back(
						"could not break a dependency cycle involving handle " + std::to_string(h)
						+ " -- no free scratch cell found; sending its move as-is, it will likely be rejected live");
					result.layerSizes.push_back(1);
					result.moves.push_back(n.mv);
					result.isScratchMove.push_back(false);
					result.unsafe = true;
					n.done = true;
					++processed;
					continue;
				}
				const auto [scratchTab, scratchX, scratchY] = *scratch;

				n.broken = true;
				scratchLayer.push_back(h);
				if (n.curTab != kUnknownTab) liveGrid(n.curTab).Clear(n.curX, n.curY, n.w, n.h);
				liveGrid(scratchTab).Mark(scratchX, scratchY, n.w, n.h, h);
				n.curTab = scratchTab;
				n.curX   = scratchX;
				n.curY   = scratchY;
				n.scratchMove = PlannedMove { h, scratchTab, scratchX, scratchY };
				for (uint64_t dep : n.dependents) {
					Node& dn = nodes[dep];
					if (--dn.inDegree == 0) currentLayer.push_back(dep);
				}
				// `n` itself is NOT done: its real move (n.mv, unchanged)
				// still has its original inDegree and will be emitted
				// normally, in a later layer, once whatever occupies its
				// true destination clears.
				++cyclesBroken;
			}

			if (!scratchLayer.empty()) {
				result.layerSizes.push_back(static_cast<uint32_t>(scratchLayer.size()));
				for (uint64_t h : scratchLayer) {
					result.moves.push_back(nodes[h].scratchMove);
					result.isScratchMove.push_back(true);
				}
			}

			if (currentLayer.empty() && processed == processedBefore) {
				// Safety net, not expected to trigger: this pass broke
				// nothing, freed nothing, and gave up on nothing, so nothing
				// will ever change from here (e.g. a dependency graph this
				// function's own bookkeeping got wrong somewhere). Emit
				// whatever's left using its ORIGINAL planned move (or, for
				// an already-broken node still awaiting its real move, that
				// real move) rather than looping forever or silently
				// dropping items -- ApplyMoves' own retry is the backstop
				// for whatever this leaves imperfect.
				std::vector<uint64_t> leftover;
				for (uint64_t h : order) if (!nodes[h].done) leftover.push_back(h);
				if (leftover.empty()) break;
				result.diagnostics.push_back(
					std::to_string(leftover.size())
					+ " move(s) could not be fully ordered (unexpected dependency shape); emitting them as-is");
				result.layerSizes.push_back(static_cast<uint32_t>(leftover.size()));
				for (uint64_t h : leftover) {
					result.moves.push_back(nodes[h].mv);
					result.isScratchMove.push_back(false);
					nodes[h].done = true;
					++processed;
				}
			}
			continue;
		}

		std::vector<uint64_t> nextLayer;
		for (uint64_t h : currentLayer) {
			Node& n = nodes[h];
			n.done  = true;
			++processed;
			result.moves.push_back(n.mv);
			result.isScratchMove.push_back(false);
			if (n.curTab != kUnknownTab) liveGrid(n.curTab).Clear(n.curX, n.curY, n.w, n.h);
			liveGrid(n.mv.destTab).Mark(n.mv.destX, n.mv.destY, n.w, n.h, h);
			n.curTab = n.mv.destTab;
			n.curX   = n.mv.destX;
			n.curY   = n.mv.destY;
			for (uint64_t dep : n.dependents) {
				Node& dn = nodes[dep];
				if (--dn.inDegree == 0) nextLayer.push_back(dep);
			}
		}
		result.layerSizes.push_back(static_cast<uint32_t>(currentLayer.size()));
		currentLayer = std::move(nextLayer);
	}

	if (cyclesBroken > 0) {
		result.diagnostics.push_back(std::to_string(cyclesBroken) + " dependency cycle(s) broken using a temporary scratch cell");
	}

	return result;
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

	// Items the SDK reports in the shared stash but with no page number are
	// in the mod's extra tabs (Reimagined's Gems / Materials / Runes), not on
	// a normal page; a live Move out of there is rejected with Unsupported.
	// Leave them out of the plan entirely.
	std::vector<ItemFacts> working;
	working.reserve(items.size());
	for (const auto& it : items) {
		if (it.sharedStashPage == kUnknownTab) {
			++plan.itemsWithoutPage;
			continue;
		}
		working.push_back(it);
	}

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

