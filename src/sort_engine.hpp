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

namespace RemoteStashSorting {

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

struct CategorySpec {
	std::string id;
	uint32_t    minTabs = 0;
};

// Mirrors SS_AUTO_LAYOUT: tabs 1-5 unique/crafted gear, tab 6 unique ring/amu,
// tab 7 rare ring/amu, tab 8 magic ring/amu, tab 9 unique charm, tabs 10-11
// magic charm, tab 12 unique jewel, tab 13 rare/magic jewel, tabs 14-19 set
// items, tab 20+ rare gear. Overridable from the plugin's TOML config.
std::vector<CategorySpec> DefaultLayout();

// True when `id` is a mode_id sort_engine.cpp knows how to run (i.e. a valid
// value for a config [[remote-stash-sorting.layout]] entry's mode_id).
bool IsKnownModeId(const std::string& id);

struct SortOutcome {
	bool                     ok = false;
	std::string              error;               // set when !ok
	uint32_t                 failedStepIndex = kUnknownTab; // index into `layout` that failed
	std::vector<PlannedMove> moves;
	uint32_t                 tabsUsed = 0;
	uint32_t                 itemsSorted = 0;
	std::vector<std::string> skippedCategories; // categories with nothing to sort
};

// tabCount is the number of *active* normal shared-stash tabs, discovered by
// Phase 2 (the SDK has no tab-count query -- see README "Open questions").
// gridWidth/gridHeight default to the save-editor's confirmed 16x13.
SortOutcome RunAutoSort(
	const std::vector<ItemFacts>&    items,
	const std::vector<CategorySpec>& layout,
	uint32_t                         tabCount,
	uint32_t                         gridWidth = 16,
	uint32_t                         gridHeight = 13);

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

}
