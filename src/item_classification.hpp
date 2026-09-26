#pragma once

// Looks up the sort_engine.hpp classification (GearSlot / NonGearKind /
// isGear / isTwoHanded / a base levelreq) for a live item's packed 4-byte
// code (D2RL::Items::ItemInfo::code). Backed by a generated static table --
// see item_classification_data.hpp and tools/gen_classification.py.
//
// This exists so plugin.cpp never needs to know the on-disk row layout of
// ItemTypes.txt / Weapons.txt / Armor.txt / Misc.txt (the SDK's
// DataTableService hands back raw, undocumented row bytes for those tables --
// see data_tables.h -- so a static, build-time-generated table keyed by item
// code is the practical alternative to either guessing a struct layout or
// reading the .txt files from disk at plugin-load time).

#include "sort_engine.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace RemoteStashSorting::Classification {

struct Facts {
	GearSlot    gearSlot     = GearSlot::None;
	NonGearKind nonGearKind  = NonGearKind::None;
	bool        isGear       = false;
	bool        isTwoHanded  = false;
	uint16_t    baseLevelReq = 0;
	uint8_t     width        = 1;
	uint8_t     height       = 1;
};

// Returns std::nullopt for an unknown code (not in weapons/armor/misc.txt --
// quest items and a handful of engine-only codes), or for a code the
// generator deliberately excluded (gems, rune, and socket-filler types,
// which no auto-sort category ever claims). Both cases mean "leave this item
// where it is": the caller should fall back to ItemFacts{} defaults, which
// makes RunAutoSort's stream matchers naturally skip it.
std::optional<Facts> Classify(uint32_t packedItemCode);

// qualityRecordId is D2RL::Items::ItemInfo::qualityRecordId, valid only for
// quality == Set. Returns an empty string when the row is out of range or
// has no usable group text -- the caller should treat that as "not part of
// a set" (an empty ItemFacts::setGroupName already makes RunAutoSort's
// atomic-set grouping skip the item, same as a completely unknown item would).
std::string SetGroupForRow(int32_t qualityRecordId);

// The TRUE required level for one item -- a full port of the save-editor's
// _ssItemSortLevel/_ssAffixLevelReq, not just the base item's own levelreq
// (see baseLevelReq above). Fixes a real problem: Rings.txt/Charms.txt/
// Jewels.txt/Amulets.txt each have exactly one row with levelreq == 0, so
// baseLevelReq alone never varies across an entire ring/charm/jewel/amulet
// category -- every "reqLevel" sort key tie there was actually being broken
// by the next key (usually "-ilvl"), which has no relationship to the
// "Required Level" shown in the item's own tooltip. This raises baseLevelReq
// using whichever of the following is higher:
//   - quality == Unique: UniqueItems.txt's own "lvl req" for row
//     qualityRecordId (item.h confirms qualityRecordId is a valid
//     UniqueItems row for Unique quality, not just SetItems for Set)
//   - quality == Set: SetItems.txt's own "lvl req" for row qualityRecordId
//   - any rolled affix: the highest "levelreq" among up to 3 prefixIds + 3
//     suffixIds (magic/rare/crafted items all populate these the same way;
//     a 0 entry means "no affix in that slot" and contributes nothing).
//     prefixIds/suffixIds are ItemInfo's own *combined* MagicSuffix+
//     MagicPrefix+AutoMagic ID space; an ID landing in the AutoMagic range
//     resolves to 0 here, same as the save-editor never consulting
//     AutoMagic.txt for this.
// qualityRecordId/prefixIds/suffixIds should be passed through verbatim from
// D2RL::Items::ItemInfo; baseLevelReq should be the Classify() result's
// baseLevelReq for this item's code (0 if the code didn't classify at all --
// a Unique/Set/affix bump can still meaningfully apply on top of that).
uint32_t TrueReqLevel(
    uint32_t quality, int32_t qualityRecordId, const uint32_t (&prefixIds)[3], const uint32_t (&suffixIds)[3], uint32_t baseLevelReq);

// Optional runtime alternative to the embedded table above: parses the
// active mod's OWN itemtypes/weapons/armor/misc/setitems.txt (from
// <modDataPath>/data/global/excel/) and switches Classify()/
// SetGroupForRow() to use that instead, so classification always matches
// whichever mod is actually running rather than a possibly-stale bundled
// reference copy. See mod_item_data.hpp for the parser and README "Config"
// for the mod_data_path setting that drives this.
//
// Returns true on success. Returns false and leaves the embedded table
// active on any failure (empty path, missing/unreadable file, malformed
// data); if `error` is non-null it receives a short human-readable reason
// the caller should log.
bool LoadFromModFolder(const std::string& modDataPath, std::string* error = nullptr);

// Reverts to the embedded, build-time-generated table. Called with an
// empty mod_data_path config value.
void UseEmbeddedTable();

// True if a mod-folder override (LoadFromModFolder) is currently active,
// false if Classify()/SetGroupForRow() are answering from the embedded
// table. For logging/diagnostics only.
bool IsUsingModFolder();

}
