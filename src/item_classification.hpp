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

}
