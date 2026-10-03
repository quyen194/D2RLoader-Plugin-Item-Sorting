#pragma once

// Single-grid sorting for the personal stash, the inventory and the Horadric
// Cube. Unlike the shared stash (sort_engine.hpp: many 16x13 tabs, one or
// more whole tabs per category), each of these containers is ONE grid, so a
// category gets an AREA of that grid instead of tabs:
//
//   - a layout entry may own a rectangular `zone`; entries with a zone are
//     placed first, and a zone that fills up spills into the rest of the
//     grid (outside every zone);
//   - every entry grows from its `anchor` corner, scanning cells in
//     `fill` order (whole columns or whole rows, starting at the anchor);
//   - with CategoryBreak::Band, consecutive zone-less entries that share an
//     anchor each start in a fresh column/row;
//   - locked cells and keep-in-place items never move and are never moved
//     into.
//
// Pure algorithm, no D2RLPlugin dependency (same as sort_engine.cpp): the
// plugin reads live items into ItemFacts, calls PlanSort(), and applies the
// returned moves. PlannedMove::destTab is always 0 here.
//
// The config file's own comments (quyen19492-item-sorting.toml, the
// personal_stash / inventory / cube sections) are the user-facing reference
// for every setting below.

#include "sort_engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ItemSorting::Grid {

inline constexpr uint32_t kMaxGridSide = 16; // D2S item coordinates are 4 bits

enum class FillDirection : uint8_t { Columns, Rows };
enum class CategoryBreak : uint8_t { None, Band };
enum class Overflow : uint8_t { Compact, Abort };
enum class Anchor : uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

// String <-> enum, using exactly the config file's spellings ("columns",
// "band", "compact", "topLeft", ...). Parse* return false for anything else.
bool ParseFillDirection(const std::string& text, FillDirection* out);
bool ParseCategoryBreak(const std::string& text, CategoryBreak* out);
bool ParseOverflow(const std::string& text, Overflow* out);
bool ParseAnchor(const std::string& text, Anchor* out);
const char* ToString(FillDirection v);
const char* ToString(CategoryBreak v);
const char* ToString(Overflow v);
const char* ToString(Anchor v);

struct Rect {
	int32_t x = 0;
	int32_t y = 0;
	int32_t w = 0;
	int32_t h = 0;
};

struct Entry {
	std::vector<std::string> modeIds;      // mode_id, or every mode_ids entry
	Anchor                   anchor = Anchor::TopLeft;
	bool                     hasZone = false;
	Rect                     zone;
	std::vector<std::string> sortKeys;     // empty = the first mode's built-in default
	std::vector<std::string> gearGroups;
};

struct Config {
	std::string              name = "grid"; // section name, for messages only
	uint32_t                 width = 10;
	uint32_t                 height = 10;
	FillDirection            fill = FillDirection::Columns;
	CategoryBreak            categoryBreak = CategoryBreak::Band;
	Overflow                 onOverflow = Overflow::Compact;
	std::vector<Rect>        lockedCells;
	std::vector<std::string> keepInPlace;
	std::vector<Entry>       layout;
};

// Built-in defaults, identical to the shipped quyen19492-item-sorting.toml
// sections. Used when a user's (older) config file has no such section --
// D2RLoader never overwrites a config file the user already has.
// section = "personal_stash" | "inventory" | "cube".
Config DefaultConfig(const std::string& section);

// Every problem found, one human-readable line each; empty = valid.
std::vector<std::string> ValidateConfig(const Config& cfg);

enum class Status : uint8_t {
	Ok,
	ConfigError, // ValidateConfig failed -- `error` has the first problem, `log` all of them
	NoRoom,      // the new arrangement doesn't fit (after the compact retry, if allowed)
};

struct Plan {
	Status                   status = Status::Ok;
	std::string              error;
	std::vector<PlannedMove> moves;              // only items whose cell actually changes
	uint32_t                 itemsArranged = 0;  // items placed by a layout entry (moved or not)
	uint32_t                 itemsFixed = 0;     // locked / keep_in_place / claimed by no entry
	uint32_t                 itemsOutsideGrid = 0; // ignored: not inside width x height
	bool                     compactRetryUsed = false;
	std::vector<std::string> log;                // diagnostic lines (one per entry, warnings)
};

Plan PlanSort(const std::vector<ItemFacts>& items, const Config& cfg);

// Turns a plan's moves (final cells only) into a LIVE-SAFE sequence: every
// step moves one item onto cells that no OTHER item occupies at that moment
// (its own old cells are fine -- the game lifts it out first, as when a
// player drops an item one cell over), exactly like moving items by hand.
// Where items block each other (swaps, chains) the blockers are first
// parked on free cells and moved on later, so a handle can appear more than
// once.
//
// Why: a live run sent a whole grid plan as one ExistingItemTransaction,
// relying on the SDK README's "moved items are removed from a temporary
// occupancy model first, so a single transaction can swap or chain their
// locations". Every move reported committed, yet items that were moved onto
// cells another item of the same plan still occupied vanished from the grid
// once that other item moved away (inventory: the cube and both tomes went
// first onto the potions' cells and disappeared; the potions, moved after,
// were fine). So the plugin now never lets the game see a move onto an
// occupied cell, and applies the steps one transaction each.
//
// `items` must be every item of the container (fixed ones included), at
// their current cells. ok = false (with `error`) if no safe order exists --
// only when the grid is so full that a blocked item fits nowhere, even
// temporarily. The whole sequence is simulated before anything is applied.
struct Sequence {
	bool                     ok = false;
	std::string              error;
	std::vector<PlannedMove> steps;
	uint32_t                 parkingMoves = 0; // temporary moves added to break blocks
};

Sequence SequenceForLiveApply(const std::vector<ItemFacts>& items, const std::vector<PlannedMove>& moves, uint32_t width, uint32_t height);

}
