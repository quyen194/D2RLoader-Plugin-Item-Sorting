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
#include "item_misc_data.hpp"           // reuses MiscEntry

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ItemSorting::Classification::ModData {

struct LoadedTable {
	// Sorted ascending by `code`, same invariant as the embedded kTable, so
	// callers can binary-search it exactly the same way.
	std::vector<ClassificationEntry> entries;

	// Index == D2RL::Items::ItemInfo::qualityRecordId for a quality==Set
	// item, same convention as the embedded kSetGroupByRow -- rows are
	// skipped exactly where the live game's own row-position numbering skips
	// them (a literal "Expansion" section-delimiter row -- see LoadFromFolder's
	// ReadTable comment), so indices line up with what qualityRecordId
	// actually assigns.
	std::vector<std::string> setGroupByRow;

	// setitems.txt's own "lvl req" column, same row indexing as
	// setGroupByRow (one array per column, same row order -- see
	// Classification::TrueReqLevel()).
	std::vector<uint16_t> setLevelReqByRow;

	// uniqueitems.txt's own "lvl req" column. Index == qualityRecordId for a
	// quality==Unique item -- item.h confirms qualityRecordId is a valid
	// UniqueItems row for Unique quality too, not just SetItems for Set.
	std::vector<uint16_t> uniqueLevelReqByRow;

	// magicprefix.txt / magicsuffix.txt's own "levelreq" column. Index+1 ==
	// the affix's table-local ID within ItemInfo::prefixIds/suffixIds'
	// *combined* MagicSuffix+MagicPrefix+AutoMagic ID space (see
	// Classification::TrueReqLevel()'s AffixLevelReq helper for the decode).
	std::vector<uint16_t> magicPrefixLevelReq;
	std::vector<uint16_t> magicSuffixLevelReq;

	// misc.txt kinds for the single-grid categories, sorted ascending by
	// `code` like the embedded kMiscTable (same rules as
	// tools/gen_misc_classification.py).
	std::vector<MiscEntry> miscEntries;
};

// Reads <modDataPath>/data/global/excel/{itemtypes,weapons,armor,misc,
// setitems,uniqueitems,magicprefix,magicsuffix}.txt and builds a LoadedTable
// with the exact same classification rules as tools/gen_classification.py
// (kept in sync by hand -- see that script's comments for the logic this
// ports).
//
// Returns std::nullopt on any failure (missing/unreadable file, malformed
// header, or a genuinely corrupt row) and, if `error` is non-null, writes a
// short human-readable reason into it. Never throws. The caller is expected
// to fall back to the embedded table and log `*error` on failure.
std::optional<LoadedTable> LoadFromFolder(const std::string& modDataPath, std::string* error);

}
