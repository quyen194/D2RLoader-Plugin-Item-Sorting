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
// The config file's own comments (quyen194-item-sorting.toml, the
// personal_stash / inventory / cube sections) are the user-facing reference
// for every setting below.

#include "sort_engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Quyen194ItemSorting::Grid {

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

// Built-in defaults, identical to the shipped quyen194-item-sorting.toml
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

}
