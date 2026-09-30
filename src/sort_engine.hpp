#pragma once

// Pure port of the auto-sort algorithm from D2R-Reimagined-Save-Editor
// (index.html: SS_AUTO_LAYOUT, SS_SORT_MODES, _ssPlanSort, _ssRunAutoSort,
// _ssPadAutoLayout, _ssInsertPaddingTab, _ssCompactEmptyTabsToEnd).
//
// This header has no dependency on D2RLPlugin or the game: it only knows
// about ItemFacts (pre-resolved sorting attributes) and produces a plan of
// PlannedMove records. Phase 2 (plugin.cpp) is responsible for:
//   - reading live items into ItemFacts (InventoryService::forEachInventoryItem
//     + DataTableService for item-type-chain / levelreq resolution)
//   - turning PlannedMove records into Items::ExistingItemOperation::Move
//     batches and calling Items::executeExistingItemTransaction
//
// One deliberate simplification vs. the save-editor: the live SDK's item
// model does not expose socketed gems/runes as separate grid-occupying
// records the way the raw .d2i byte format does (ItemInfo just reports
// socketCount/socketedItemCount on the parent), so there is no
// findSocketedChildren() equivalent here -- moving the parent item handle is
// enough.

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace Quyen194ItemSorting {

inline constexpr uint32_t kUnknownTab = (std::numeric_limits<uint32_t>::max)();

enum class GearSlot : uint8_t {
	None = 0,
	Helm, Torso, Belt, Gloves, Boots, Shield,
	Axe, Sword, Knife, ClubMace, Scepter, Wand, Staff, Orb,
	Spear, Polearm, Javelin, Bow, Arrows, Crossbow, Bolts, Claw,
	OtherWeapon,
};

enum class NonGearKind : uint8_t {
	None = 0, Charm, Jewel, Ring, Amulet,
};

// Finer kind for the misc.txt items the single-grid containers (personal
// stash / inventory / cube -- see grid_sort.hpp) care about, resolved from
// misc.txt's own `type` column through itemtypes.txt's Equiv chain (see
// item_classification.hpp's ClassifyMisc and tools/gen_misc_classification.py).
// Independent of isGear/gearSlot/nonGearKind above: the embedded gear table
// keeps classifying potions/quest items as isGear=true, GearSlot::None
// exactly as before, so the shared-stash engine's behavior is unchanged.
enum class MiscKind : uint8_t {
	None = 0,
	Rune,   // itemtype chain "rune", plus Reimagined's rune stacks ("runx")
	Gem,    // itemtype chain "gem", plus Reimagined's gem stacks ("gemx")
	Potion, // itemtype chain "poti" (healing/mana/rejuv/stamina/antidote/thawing)
	Tool,   // Horadric Cube ("box"), tomes, scrolls, keys, storage/grabber items
	Quest,  // itemtype chain "ques" / "body" (minus the Horadric Cube)
};

// Facts about one item, already resolved by Phase 2 from ItemInfo plus data
// tables. quality uses the same numeric values as D2RL::Items::Quality
// (Magic=4, Set=5, Rare=6, Unique=7, Crafted=8) so callers can cast directly.
struct ItemFacts {
	uint64_t    handle = 0; // opaque D2RL::Items::ItemHandle, round-tripped only
	uint32_t    code = 0;   // packed 4-byte item type code (logging / tie-break)
	uint32_t    quality = 0;
	uint32_t    itemLevel = 0;
	uint32_t    sortLevel = 0; // base levelreq raised by quality/affix, mirrors _ssItemSortLevel
	uint32_t    sharedStashPage = kUnknownTab;
	int32_t     x = 0;
	int32_t     y = 0;
	uint32_t    w = 1;
	uint32_t    h = 1;
	bool        isGear = false;
	bool        isTwoHanded = false;
	GearSlot    gearSlot = GearSlot::None;
	NonGearKind nonGearKind = NonGearKind::None;
	// Single-grid sort keys only (see MiscKind). miscOrder is the rune number
	// (El = 1 ... Zod = 33), the gem type (1 = amethyst ... 8 = chaos onyx) or
	// the potion type (1 = healing ... 6 = thawing); miscGrade is the gem
	// grade (1 = chipped ... 5 = perfect, 6 = Reimagined's special gem, 7 =
	// stack) or the potion grade (1 = minor/small ... 6 = full). 0 = n/a.
	MiscKind    miscKind = MiscKind::None;
	uint16_t    miscOrder = 0;
	uint16_t    miscGrade = 0;
	int32_t     setId = -1; // set/unique row id; -1 when not applicable
	std::string setGroupName; // for atomic set grouping/ordering; empty if not a set item
	std::string baseNameKey;  // tie-break text; comparison falls back to `code` when empty
};

struct PlannedMove {
	uint64_t handle = 0;
	uint32_t destTab = kUnknownTab;
	int32_t  destX = 0;
	int32_t  destY = 0;
};

// "never" (rule never fires), or a 0-100 fill-percentage threshold ("always"
// from a config file is just threshold 0 -- see LoadConfig/_ssNormEmptyPageRule
// parity note in plugin.cpp). Mirrors the save-editor's addEmptyPageWhen.
struct EmptyPageRule {
	bool   enabled = false;
	double percentThreshold = 30.0; // fill% above which a breathing tab is added

	static EmptyPageRule Never() { return {}; }
	static EmptyPageRule AtPercent(double pct) { return { true, pct }; }
};

struct CategorySpec {
	std::string    id;
	uint32_t       minTabs = 0;
	bool           hasEmptyPageOverride = false; // false = inherit AutoSortConfig::addEmptyPageWhen
	EmptyPageRule  emptyPageOverride;

	// This category's sort order, most-significant key first; a "-" prefix on
	// a key name means descending. Empty = use this mode's own built-in
	// default (see sort_engine.cpp's BuiltinModes -- every mode ships a
	// default list that reproduces this plugin's original, pre-config-driven
	// ordering exactly, so an empty list here is never "unsorted").
	std::vector<std::string> sortKeys;

	// Gear categories only: this category's own priority order for the
	// "gearGroup" sort key, as a list of the 23 slot/weapon-type keys (see
	// GearSlot / the config file's own comment for the full key list). A key
	// left out keeps its default relative position, behind every key that IS
	// listed. Empty = the default (GearSlot's own declaration order).
	std::vector<std::string> gearGroups;
};

// One category's placement in the tab sequence, filled in by RunAutoSort for
// the padding pass below to work from.
struct CategorySpan {
	std::string id;
	uint32_t    start = 0;      // first tab index in this category's run
	uint32_t    end = 0;        // one past the last tab index (== cursor after)
	uint32_t    tabsUsed = 0;   // end - start, kept in sync as padding grows it
	bool        matchedAnything = false;
};

// Mirrors SS_AUTO_LAYOUT: tabs 1-5 unique/crafted gear, tab 6 unique ring/amu,
// tab 7 rare ring/amu, tab 8 magic ring/amu, tab 9 unique charm, tabs 10-11
// magic charm, tab 12 unique jewel, tab 13 rare/magic jewel, tabs 14-19 set
// items, tab 20+ rare gear. Overridable from the plugin's TOML config.
std::vector<CategorySpec> DefaultLayout();

// True when `id` is a mode_id sort_engine.cpp knows how to run (i.e. a valid
// value for a config [[item-sorting.layout]] entry's mode_id).
bool IsKnownModeId(const std::string& id);

// --- Building blocks shared with the single-grid engine (grid_sort.hpp) ---

// True if `it` belongs to mode `id`. Always false for an unknown id and for
// the catch-all "unclassified" (whose membership depends on the rest of the
// layout -- the caller decides that).
bool ModeMatchesItem(const std::string& id, const ItemFacts& it);

// True for "unclassified".
bool IsCatchAllMode(const std::string& id);

// True for the gear modes (set, uniqueGear, rareGear, magicGear,
// superiorGear, normalGear). grid_sort.cpp uses this to keep misc.txt items
// that the gear table marks isGear=true with GearSlot::None (potions, quest
// items, Reimagined's orbs/stacks/dyes) out of gear categories.
bool IsGearModeId(const std::string& id);

// True if `name` (without any "-" prefix) is a sort key this engine
// resolves. Unknown names are silently ignored by the shared-stash engine;
// the single-grid config treats them as a config error instead.
bool IsKnownSortKey(const std::string& name);

// The built-in sort_keys a mode uses when its config entry sets none.
// Empty for an unknown id.
std::vector<std::string> DefaultSortKeysForMode(const std::string& id);

// Strict-weak "a sorts before b" for one category: `sortKeys` (most
// significant first, "-" = descending) and `gearGroups` (the gearGroup key's
// order) exactly as a [[layout]] entry spells them. Ties keep input order if
// the caller uses std::stable_sort.
std::function<bool(const ItemFacts&, const ItemFacts&)> MakeItemOrder(
	const std::vector<std::string>& sortKeys, const std::vector<std::string>& gearGroups);

struct SortOutcome {
	bool                       ok = false;
	std::string                error;               // set when !ok
	uint32_t                   failedStepIndex = kUnknownTab; // index into `layout` that failed
	std::vector<PlannedMove>   moves;
	uint32_t                   tabsUsed = 0;
	uint32_t                   itemsSorted = 0;
	std::vector<std::string>   skippedCategories; // categories with nothing to sort
	std::vector<std::string>   categoryLog; // one line per category: tab span + item count (diagnostic)
	std::vector<CategorySpan>  spans; // one per layout entry, in order -- feeds the padding pass
};

// tabCount is the number of *active* normal shared-stash tabs, discovered by
// Phase 2 (the SDK has no tab-count query -- see README "Open questions").
// gridWidth/gridHeight default to the save-editor's confirmed 16x13. startTab
// reserves that many leading tabs: the layout's first category starts at
// tab `startTab` instead of tab 0, and nothing in this call ever targets a
// tab below it (mirrors the save-editor's startTab).
SortOutcome RunAutoSort(
	const std::vector<ItemFacts>&    items,
	const std::vector<CategorySpec>& layout,
	uint32_t                         tabCount,
	uint32_t                         gridWidth = 16,
	uint32_t                         gridHeight = 13,
	uint32_t                         startTab = 0);

// Independent post-hoc sanity check: re-derives each move's item footprint
// from `items` (by handle) and replays every move into a fresh per-tab grid,
// in the same order they appear in `moves`, flagging any move whose target
// cells are already occupied by an earlier move or by a pre-existing item
// this plan never touches. Returns one human-readable description per
// collision found (capped), empty if the plan is clean. This exists purely
// to tell planning-side bugs (RunAutoSort/classification data) apart from
// application-side ones (ApplyMoves/the live transaction) when a sort looks
// wrong in-game -- see README "Known gaps".
std::vector<std::string> DetectCollisions(
	const std::vector<ItemFacts>&    items,
	const std::vector<PlannedMove>&  moves,
	uint32_t                         gridWidth = 16,
	uint32_t                         gridHeight = 13);

// Turns a *planned* move list (which only has to be correct about final
// positions -- see RunAutoSort/RunAutoSortWithPadding) into a live-safe
// *execution* order.
//
// The planner places every item into a layout it builds fresh, category by
// category, with no regard for what is CURRENTLY sitting in each destination
// cell live in-game. Since a full Auto Sort typically repositions most or
// all of the stash, a huge fraction of destinations are, at the moment
// ApplyMoves would naively try to move into them, still occupied by some
// OTHER item this same plan is also about to move elsewhere -- the live
// Move API rejects that with Items::Result::Conflict, and no amount of
// re-trying a single fixed order fixes it: item A can't move into B's old
// slot until B has actually moved out first.
//
// This does what a save-editor byte-rewrite never had to: it collapses the
// plan to one net move per handle (only the LAST planned move for a handle
// is where it actually ends up -- same reasoning DetectCollisions already
// uses), builds a dependency graph from *live* current occupancy (`items`)
// against each move's destination footprint, and topologically sorts it so
// that whichever item currently sits in a destination always moves out
// first. A destination cell can depend on more than one occupant if the
// item moving in is larger than 1x1 and straddles more than one currently
// distinct item.
//
// A cycle (A wants B's spot, B wants A's spot -- or a longer loop of the
// same shape) has no valid one-item-at-a-time order at all without a spare
// cell, so one member of each cycle found is temporarily relocated to a
// scratch cell (any cell this function can prove will never be touched by
// anything else: not currently occupied by any live item, and not any
// move's own final destination) to break the cycle, then moved on to its
// real destination once the rest of the cycle has resolved. `diagnostics`
// carries one human-readable note per such cycle break, per otherwise-
// unresolvable dependency (something outside this plan is squatting on a
// destination), and per cycle that could not be broken for lack of a free
// scratch cell (vanishingly rare outside an almost-completely-full stash) --
// all non-fatal; the caller logs them and applies whatever `moves` contains.
//
// Every entry in the returned `moves` is a *live* move: real ones (one per
// input handle, verbatim) plus, only where a cycle needed breaking, one
// extra synthetic "move to scratch" ahead of that handle's real move.
// Applying them to the live game strictly in this order (still safe to
// chunk/bisect on failure -- ApplyMoves' own retry stays as a backstop for
// anything this couldn't foresee, e.g. stale facts) should encounter
// Conflict only for genuinely unresolvable cases.
struct SequencedMoves {
	std::vector<PlannedMove> moves;
	std::vector<std::string> diagnostics;

	// `moves` grouped into topological layers: every move in one layer is
	// independent of every OTHER move in that same layer (neither depends on
	// the other), so a layer is safe to send to the live game as a single
	// batch -- but a move can depend on one from an EARLIER layer, so layers
	// must be applied to the live game strictly in order, only starting the
	// next layer's call(s) once the previous layer's have actually
	// committed. layerSizes.size() == number of layers, and its entries sum
	// to moves.size(); layer i is moves[offset..offset+layerSizes[i]) where
	// offset is the running sum of the sizes before it.
	//
	// This exists because the live SDK's own doc comment for
	// ExistingItemTransactionResult is ambiguous about whether operations
	// within a single transaction call can see each other's effects (it
	// documents a `failureIndex`, suggesting in-order attempts, but also
	// that committedOperationCount is zero on any failure, i.e. a full
	// rollback) -- a caller that only cares about correctness, not
	// squeezing every dependency into one call, should split on layer
	// boundaries and never batch across them; how many of a layer's own
	// moves it dares to combine into one call (all of them, one each, or
	// something in between) is a call-batching decision for that caller
	// (see plugin.cpp's ApplyMoves), not something this ordering makes for
	// it.
	std::vector<uint32_t> layerSizes;

	// Parallel to `moves` (same size, same order): true for a synthetic
	// "move to scratch cell" this function inserted to break a dependency
	// cycle (see the cycle-break comment above `moves`), false for an
	// item's real, planned destination. A caller logging a rejected move
	// should say which this was -- a live report of `Result::Unsupported`
	// rejections clustering on a handful of specific, otherwise-untouched
	// tab numbers turned out to be exactly this: FindScratchCell (see the
	// .cpp) picks the first free cell starting from tab 0, but a busy stash
	// can easily have no free cell left anywhere in the tabs the plan
	// itself actually uses, spilling scratch picks into tabs past that --
	// tabs the plan never places a single real item into, and (unconfirmed,
	// see README "Known gaps" #3/#8) possibly not "materialized" for a live
	// Move if the player has never opened them either. This flag is what
	// lets that be told apart, in the log, from a real placement failing.
	std::vector<bool> isScratchMove;
};

// preferredMaxTab (optional; kUnknownTab means "no preference") biases
// FindScratchCell (see the .cpp) to look for a scratch cell within
// [0, preferredMaxTab) first, falling back to the rest of [0, tabCount) only
// if that narrower range has nothing free -- it never makes a scratch pick
// fail that would otherwise succeed. Pass the caller's own already-known
// "real" active tab count (e.g. AutoSortPlan::tabsUsed) here: those tabs are
// proven safe for a live Move (the plan just placed real items there), while
// tabs beyond them (padding the stash has but Auto Sort never touches) are
// the ones a scratch pick would otherwise land in by default -- see
// isScratchMove's own comment for why that specifically has caused live
// rejections.
SequencedMoves SequenceMovesForLiveApply(
	const std::vector<ItemFacts>&   items,
	const std::vector<PlannedMove>& moves,
	uint32_t                        tabCount,
	uint32_t                        gridWidth       = 16,
	uint32_t                        gridHeight      = 13,
	uint32_t                        preferredMaxTab = kUnknownTab);

// "Add blank page": slide the nearest empty tab at or after `afterTab` so it
// lands immediately after `afterTab`, by shifting every tab in between back
// one slot (pure tab-content rotation, same trick as the save-editor's
// _ssInsertPaddingTab / _ssCompactEmptyTabsToEnd, done here with real Move
// operations instead of byte splicing). Only tabs *after* afterTab are
// eligible donors, matching the save-editor's borrow rule.
struct ShiftOutcome {
	bool                     ok = false;
	std::string              error;
	std::vector<PlannedMove> moves;
	uint32_t                 blankTab = kUnknownTab; // final tab index of the moved blank
};

ShiftOutcome MoveBlankTabAfter(
	const std::vector<ItemFacts>& items,
	uint32_t                      afterTab,
	uint32_t                      tabCount);

// "Remove current page": the mirror operation. Caller/UI must already have
// confirmed currentTab is empty (the button is disabled otherwise); this
// pushes it to the end by shifting every following tab forward one slot.
ShiftOutcome PushBlankTabToEnd(
	const std::vector<ItemFacts>& items,
	uint32_t                      currentTab,
	uint32_t                      tabCount);

// --- Auto Sort orchestration: compact / pad / blank-at-start ------------
//
// Mirrors the save-editor's autosort.json layer (startTab, blankTabsAtStart,
// compactEmptyTabsFirst, addEmptyPageWhen) end to end: RunAutoSortWithPadding
// runs RunAutoSort, then (optionally) inserts one "breathing" blank tab
// after any category whose last tab is fuller than its threshold, then
// (optionally) pulls blankTabsAtStart empty tabs up to the front. Every
// phase reads a local working copy of `items` that is kept in sync with the
// previous phase's planned moves, so e.g. the padding pass sees tabs exactly
// where the sort actually left them, and blank-at-start sees them exactly
// where padding left them -- all without touching the live game until the
// caller applies the final combined `moves` list in one transaction.
//
// Not ported: the save-editor's quota-shortfall padding pass
// (_ssPadAutoLayout's pass 1 / the `padding: roundRobin|inOrder|off` knob).
// That pass exists there because an empty category reserves *nothing* up
// front and has to be handed its quota after the fact; RunAutoSort here
// always reserves a category's minTabs immediately (see its own comment),
// so an empty category already ends up with minTabs left blank in place --
// there is no shortfall left for a pass 1 to redistribute. Only the
// "breathing page" pass (pass 2 / addEmptyPageWhen) applies.
struct AutoSortConfig {
	std::vector<CategorySpec> layout;
	uint32_t                  tabCount = 0;
	uint32_t                  gridWidth = 16;
	uint32_t                  gridHeight = 13;
	uint32_t                  startTab = 0;
	uint32_t                  blankTabsAtStart = 0;
	bool                      compactEmptyTabsFirst = false;
	EmptyPageRule             addEmptyPageWhen; // global default; a category can override via CategorySpec
};

struct AutoSortPlan {
	bool                       ok = false;
	std::string                error;
	uint32_t                   failedStepIndex = kUnknownTab;
	std::vector<PlannedMove>   moves; // every phase's moves, in the order they must be applied
	uint32_t                   itemsSorted = 0;
	uint32_t                   tabsUsed = 0; // end of the category layout, before padding/blank-at-start
	std::vector<std::string>   skippedCategories;
	std::vector<std::string>   categoryLog;
	uint32_t                   compactMoveCount = 0;    // item moves spent relabeling empty tabs to the end
	uint32_t                   breathingTabsAdded = 0;  // categories that got a pass-2 padding tab
	uint32_t                   blankTabsAtStartAdded = 0; // of cfg.blankTabsAtStart, how many actually landed
};

AutoSortPlan RunAutoSortWithPadding(
	const std::vector<ItemFacts>& items,
	const AutoSortConfig&         cfg);

// Pure relabeling: slides every empty tab in [startTab, tabCount) to the end
// of that range, keeping the relative order of the non-empty ones (and of
// the empty ones) otherwise unchanged. No item's x/y changes, only which tab
// number it is in -- mirrors _ssCompactEmptyTabsToEnd without the
// save-editor's physical-byte-page constraint (a "tab" is just a number
// here). Exposed separately from RunAutoSortWithPadding so a caller can run
// it on its own (e.g. before a manual sort) if useful.
std::vector<PlannedMove> CompactEmptyTabsToEnd(
	const std::vector<ItemFacts>& items,
	uint32_t                      startTab,
	uint32_t                      tabCount);

}
