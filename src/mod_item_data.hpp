#pragma once

// Optional runtime alternative to the embedded, build-time-generated
// item_classification_data.hpp table: parses a mod's OWN
// itemtypes/weapons/armor/misc/setitems.txt directly off disk, at plugin
// load/config-load time, so item classification always matches whichever
// mod is actually active instead of a possibly-stale bundled reference copy
// (see README "Known gaps" for the concrete bug this was written to fix --
// a live report showed the embedded table's SetItems.txt row numbering had
// drifted from the game's own qualityRecordId numbering).
//
// This file has no PluginSDK dependency (same as item_classification.cpp/
// sort_engine.cpp) so it can be unit-tested with a plain g++ build against
// real *.txt fixtures, independent of the Windows/MSVC-only plugin.cpp.

#include "item_classification_data.hpp" // reuses ClassificationEntry

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RemoteStashSorting::Classification::ModData {

struct LoadedTable {
	// Sorted ascending by `code`, same invariant as the embedded kTable, so
	// callers can binary-search it exactly the same way.
	std::vector<ClassificationEntry> entries;

	// Index == D2RL::Items::ItemInfo::qualityRecordId for a quality==Set
	// item, same convention as the embedded kSetGroupByRow (including
	// skipping fully-blank interior rows so indices line up with what the
	// live game actually assigns -- see the loader's own comment).
	std::vector<std::string> setGroupByRow;
};

// Reads <modDataPath>/data/global/excel/{itemtypes,weapons,armor,misc,
// setitems}.txt and builds a LoadedTable with the exact same classification
// rules as tools/gen_classification.py (kept in sync by hand -- see that
// script's comments for the logic this ports).
//
// Returns std::nullopt on any failure (missing/unreadable file, malformed
// header, or a genuinely corrupt row) and, if `error` is non-null, writes a
// short human-readable reason into it. Never throws. The caller is expected
// to fall back to the embedded table and log `*error` on failure.
std::optional<LoadedTable> LoadFromFolder(const std::string& modDataPath, std::string* error);

}
