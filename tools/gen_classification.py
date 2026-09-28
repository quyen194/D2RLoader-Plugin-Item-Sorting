#!/usr/bin/env python3
"""Generates item_classification_data.hpp for the Quyen194ItemSorting plugin.

Ports the classification logic from D2R-Reimagined-Save-Editor/index.html:
  - buildItemTypeChains() / ITEM_ITYPE_CHAIN  (itemtypes.txt Equiv1/Equiv2 walk)
  - buildSocketAndTypeMap() / ITEM_TYPE_BY_CODE (weapons/armor/misc 'type' column)
  - buildWeaponAndBeltMeta() / IS_2HANDED (weapons.txt '2handed' column)
  - SS_GEAR_GROUPS / SS_NON_GEAR_TYPES / _ssIsGearItem / _ssGearGroupIndex

This runs once, offline, at plugin-development time (not at plugin runtime).
Its output is a static C++ table baked into the plugin DLL, keyed by the
item's packed 4-byte code (same encoding as D2RL::Items::MakeItemCode /
ItemInfo.code), so plugin.cpp needs no DataTableService row-layout knowledge
(the SDK does not publish ItemTypes/Weapons/Armor/Misc row struct layouts).

v1 simplification (see README "Known gaps"): sortLevel only carries the base
item's own levelreq column. It does NOT add the unique/set 'lvl req' bump or
the rolled affix levelreq bump the save-editor's _ssItemSortLevel applies,
because those need UniqueItems/SetItems/MagicAffixes row data the SDK does
not expose a typed accessor for either. itemLevel (live, from ItemInfo) is
still the primary numeric tie-break ahead of it in every sort key, so the
practical impact is small.
"""
import re
from pathlib import Path

BASE = Path("/mnt/user-data/uploads/D2R-Reimagined-Save-Editor/base")
OUT = Path("/home/claude/item_classification_data.hpp")


def read_table(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = [ln.rstrip("\r\n") for ln in text.split("\n") if ln.strip("\r\n") != ""]
    header = lines[0].split("\t")
    rows = []
    for ln in lines[1:]:
        cells = ln.split("\t")
        row = {}
        for i, h in enumerate(header):
            row[h] = cells[i] if i < len(cells) else ""
        rows.append(row)
    return rows


itemtypes = read_table(BASE / "itemtypes.txt")
weapons = read_table(BASE / "weapons.txt")
armor = read_table(BASE / "armor.txt")
misc = read_table(BASE / "misc.txt")
setitems = read_table(BASE / "setitems.txt")

# ---- buildItemTypeChains ----
equiv = {}
for row in itemtypes:
    code = (row.get("Code") or "").strip()
    if not code:
        continue
    eq1 = (row.get("Equiv1") or "").strip()
    eq2 = (row.get("Equiv2") or "").strip()
    equiv[code] = [e for e in (eq1, eq2) if e]


def chain(code, visited):
    if code in visited:
        return set()
    visited.add(code)
    out = {code}
    for eq in equiv.get(code, []):
        out |= chain(eq, visited)
    return out


ITEM_ITYPE_CHAIN = {code: chain(code, set()) for code in equiv.keys()}

# ---- buildSocketAndTypeMap ----
ITEM_TYPE_BY_CODE = {}
LEVELREQ_BY_CODE = {}
SIZE_BY_CODE = {}
for src in (weapons, armor, misc):
    for row in src:
        code = (row.get("code") or "").strip()
        if not code:
            continue
        t = (row.get("type") or "").strip()
        if t:
            ITEM_TYPE_BY_CODE[code] = t
        try:
            lvl = int((row.get("levelreq") or "0").strip() or "0")
        except ValueError:
            lvl = 0
        LEVELREQ_BY_CODE[code] = lvl
        try:
            w = int((row.get("invwidth") or "1").strip() or "1")
        except ValueError:
            w = 1
        try:
            h = int((row.get("invheight") or "1").strip() or "1")
        except ValueError:
            h = 1
        SIZE_BY_CODE[code] = (max(1, w), max(1, h))

# ---- setitems.txt: row position (0-based, skipping genuinely-unused rows)
# -> raw 'set' column ---------------------------------------------------
# D2RL::Items::ItemInfo.qualityRecordId is "a zero-based SetItems row" for
# quality==Set items, i.e. exactly this row position (confirmed by the
# save-editor's _buildSetItemCatalog comment: "Row position (idx) is the
# setId ... the game uses row position, not the *ID column"). The raw 'set'
# column text (unlocalized) is used as the group key -- it is only ever
# compared to itself (map grouping + alphabetical ordering in RunAutoSort's
# atomic-set placement), never shown to the player, so no localization table
# is needed here.
#
# CONFIRMED BUG (live report, 2026-09-26): setitems.txt's base data has
# exactly one fully-blank interior row (both 'item' and 'set' empty -- a
# genuinely unused/reserved slot in Blizzard's own data, not a parsing
# artifact -- row 62 of 456 in this build's base/setitems.txt, immediately
# before "Natalya's Odium"'s 4 rows). The live game does NOT hand out a
# qualityRecordId for that unused row -- it counts only real, usable set-item
# rows -- so every row from there on is one position *ahead* of this table's
# old raw 1:1 file-row indexing. A live Grim Helm confirmed this exactly:
# it reported qualityRecordId=62, which used to be this table's blank row,
# one *before* the real "Natalya's Odium" entries (old index 63). Fix: skip
# emitting a slot at all for a fully-blank row, instead of emitting "" and
# still consuming a slot -- this keeps every later row's position aligned
# with what the live game actually assigns. (If a *different* row somewhere
# has only one of item/set set rather than both, that is a different,
# unconfirmed case -- keep giving that row its own "" slot rather than
# guessing it should be skipped too.)
SET_GROUP_BY_ROW = []
for row in setitems:
    item = (row.get("item") or "").strip()
    set_group = (row.get("set") or "").strip()
    if not item and not set_group:
        continue
    SET_GROUP_BY_ROW.append(set_group if set_group else "")

# ---- buildWeaponAndBeltMeta: IS_2HANDED ----
IS_2HANDED = set()
for row in weapons:
    code = (row.get("code") or "").strip()
    if code and (row.get("2handed") or "").strip() == "1":
        IS_2HANDED.add(code)

# ---- SS_GEAR_GROUPS (order matches GearSlot enum: None=0, then 1..23) ----
SS_GEAR_GROUPS = [
    ("Helm", ["helm"]),
    ("Torso", ["tors"]),
    ("Belt", ["belt"]),
    ("Gloves", ["glov"]),
    ("Boots", ["boot"]),
    ("Shield", ["shld"]),
    ("Axe", ["axe"]),
    ("Sword", ["swor"]),
    ("Knife", ["knif"]),
    ("ClubMace", ["club", "mace", "hamm"]),
    ("Scepter", ["scep"]),
    ("Wand", ["wand"]),
    ("Staff", ["staf"]),
    ("Orb", ["orb"]),
    ("Spear", ["spea"]),
    ("Polearm", ["pole"]),
    ("Javelin", ["jave"]),
    ("Bow", ["bow"]),
    ("Arrows", ["bowq"]),
    ("Crossbow", ["xbow"]),
    ("Bolts", ["xboq"]),
    ("Claw", ["h2h"]),
    ("OtherWeapon", ["weap"]),
]
SS_NON_GEAR_TYPES = {"char", "torc", "jewl", "ring", "amul", "gem", "sock"}


def type_chain_has(base_type, wanted):
    if not base_type:
        return False
    ch = ITEM_ITYPE_CHAIN.get(base_type, {base_type})
    return any(w in ch for w in wanted)


def classify(code):
    base_type = ITEM_TYPE_BY_CODE.get(code, "")
    if not base_type:
        return None  # untyped code (not weapons/armor/misc, e.g. quest items) -- skip

    if type_chain_has(base_type, ["char", "torc"]):
        return ("None", "Charm", False, False)
    if type_chain_has(base_type, ["jewl"]):
        return ("None", "Jewel", False, False)
    if type_chain_has(base_type, ["ring"]):
        return ("None", "Ring", False, False)
    if type_chain_has(base_type, ["amul"]):
        return ("None", "Amulet", False, False)
    if type_chain_has(base_type, ["gem", "sock"]):
        return None  # gems / socket fillers: never touched by any sort category

    # isGear (mirrors _ssIsGearItem): typed, and not one of the non-gear types.
    gear_slot = "None"
    for slot_name, types in SS_GEAR_GROUPS:
        if type_chain_has(base_type, types):
            gear_slot = slot_name
            break
    is_two_handed = code in IS_2HANDED
    return (gear_slot, "None", True, is_two_handed)


def make_item_code(code4):
    code4 = (code4 + "    ")[:4]
    return sum(ord(c) << (8 * i) for i, c in enumerate(code4))


entries = []
for code in sorted(ITEM_TYPE_BY_CODE.keys()):
    result = classify(code)
    if result is None:
        continue
    gear_slot, non_gear_kind, is_gear, is_two_handed = result
    levelreq = LEVELREQ_BY_CODE.get(code, 0)
    w, h = SIZE_BY_CODE.get(code, (1, 1))
    packed = make_item_code(code)
    entries.append((packed, code, gear_slot, non_gear_kind, is_gear, is_two_handed, levelreq, w, h))

entries.sort(key=lambda e: e[0])

# Sanity: packed codes must be unique (they're the lookup key / binary-search input).
seen = {}
dupes = []
for e in entries:
    if e[0] in seen:
        dupes.append((e, seen[e[0]]))
    seen[e[0]] = e
if dupes:
    raise SystemExit(f"duplicate packed codes: {dupes[:5]} (total {len(dupes)})")

lines = []
lines.append("#pragma once")
lines.append("")
lines.append("// GENERATED FILE -- do not edit by hand.")
lines.append("// Produced by gen_classification.py from D2R-Reimagined-Save-Editor's")
lines.append("// base/{itemtypes,weapons,armor,misc}.txt (see that script for the exact")
lines.append("// port of buildItemTypeChains/buildSocketAndTypeMap/SS_GEAR_GROUPS).")
lines.append("//")
lines.append(f"// {len(entries)} classified item codes.")
lines.append("")
lines.append("#include \"sort_engine.hpp\"")
lines.append("#include <cstdint>")
lines.append("")
lines.append("namespace Quyen194ItemSorting::Classification {")
lines.append("")
lines.append("struct ClassificationEntry {")
lines.append("\tuint32_t    code; // packed 4-byte item code, ascending, for binary search")
lines.append("\tGearSlot    gearSlot;")
lines.append("\tNonGearKind nonGearKind;")
lines.append("\tbool        isGear;")
lines.append("\tbool        isTwoHanded;")
lines.append("\tuint16_t    baseLevelReq;")
lines.append("\tuint8_t     width;  // inventory footprint, from *.txt invwidth")
lines.append("\tuint8_t     height; // inventory footprint, from *.txt invheight")
lines.append("};")
lines.append("")
lines.append(f"inline constexpr ClassificationEntry kTable[{len(entries)}] = {{")
for packed, code, gear_slot, non_gear_kind, is_gear, is_two_handed, levelreq, w, h in entries:
    comment = code.strip() or code
    lines.append(
        f"\t{{ 0x{packed:08X}U, GearSlot::{gear_slot}, NonGearKind::{non_gear_kind}, "
        f"{'true' if is_gear else 'false'}, {'true' if is_two_handed else 'false'}, "
        f"{levelreq}u, {w}u, {h}u }}, // {comment}"
    )
lines.append("};")
lines.append("")


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


lines.append("// Index == D2RL::Items::ItemInfo::qualityRecordId for a quality==Set item.")
lines.append("// Empty string means \"row has no usable set/item text\" (skip grouping).")
lines.append(f"inline constexpr const char* kSetGroupByRow[{len(SET_GROUP_BY_ROW)}] = {{")
for group in SET_GROUP_BY_ROW:
    lines.append(f'\t"{esc(group)}",')
lines.append("};")
lines.append("")
lines.append("} // namespace Quyen194ItemSorting::Classification")
lines.append("")

OUT.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {OUT} with {len(entries)} entries")
