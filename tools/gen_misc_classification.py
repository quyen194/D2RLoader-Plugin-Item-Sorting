#!/usr/bin/env python3
"""Generates src/item_misc_data.hpp: the misc.txt item kinds used by the
single-grid sort categories rune / gem / potion / tool / quest (see
MiscKind in src/sort_engine.hpp and src/grid_sort.hpp).

Reads <excel dir>/{itemtypes,misc}.txt -- normally the active mod's own
data/global/excel folder, e.g.
    python tools/gen_misc_classification.py "<game>/mods/Reimagined/Reimagined.mpq/data/global/excel"

The same rules run at plugin load time when mod_data_path is set
(src/mod_item_data.cpp, BuildMiscEntries) -- keep the two in sync.

Rules (first match wins), per misc.txt row:
  code "box"                                   -> tool   (Horadric Cube; its type is "ques")
  type chain has "rune"                        -> rune   order = NN from "rNN"
  type "runx" (Reimagined rune stack)          -> rune   order = NN from "sNN", grade 1 (stack)
  type chain has "gem"                         -> gem    order = gem type, grade from type2
  type "gemx" (Reimagined gem stack)           -> gem    order from the stack code, grade 7
  type chain has "poti"                        -> potion order = potion type, grade from the code
  type chain has book/scro/key/stor/grab       -> tool
  type chain has "ques" or "body"              -> quest
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "src" / "item_misc_data.hpp"

GEM_TYPE_ORDER = {"gema": 1, "gemt": 2, "gems": 3, "geme": 4, "gemr": 5, "gemd": 6, "gemz": 7, "gemo": 8}
GEM_STACK_ORDER = {"1am": 1, "1tp": 2, "1sa": 3, "1em": 4, "1rb": 5, "1di": 6, "1sk": 7, "1so": 8}
GEM_GRADE = {"gem0": 1, "gem1": 2, "gem2": 3, "gem3": 4, "gem4": 5, "pgem": 6}
GEM_STACK_GRADE = 7
POTION_TYPE_ORDER = {"hpot": 1, "mpot": 2, "rpot": 3, "spot": 4, "apot": 5, "wpot": 6}
TOOL_TYPES = ("book", "scro", "key", "stor", "grab")


def read_table(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = [ln.rstrip("\r\n") for ln in text.split("\n") if ln.strip("\r\n") != ""]
    header = lines[0].split("\t")
    rows = []
    for ln in lines[1:]:
        cells = ln.split("\t")
        if cells and cells[0] == "Expansion":
            continue
        rows.append({h: (cells[i] if i < len(cells) else "") for i, h in enumerate(header)})
    return rows


def two_digits(code: str, prefix: str) -> int:
    if len(code) == 3 and code[0] == prefix and code[1:].isdigit():
        return int(code[1:])
    return 0


def potion_grade(code: str) -> int:
    last = code[-1:] if code else ""
    if last.isdigit():
        return int(last)
    return {"f": 6, "l": 2, "s": 1, "o": 3}.get(last, 0)


def classify(code, base_type, type2, chain):
    """-> (kind, order, grade) or None. Mirrors mod_item_data.cpp BuildMiscEntries."""
    if code == "box":
        return ("Tool", 0, 0)
    if "rune" in chain:
        return ("Rune", two_digits(code, "r"), 0)
    if base_type == "runx":
        return ("Rune", two_digits(code, "s"), 1)
    if "gem" in chain:
        return ("Gem", GEM_TYPE_ORDER.get(base_type, 0), GEM_GRADE.get(type2, 0))
    if base_type == "gemx":
        return ("Gem", GEM_STACK_ORDER.get(code, 0), GEM_STACK_GRADE)
    if "poti" in chain:
        return ("Potion", POTION_TYPE_ORDER.get(base_type, 0), potion_grade(code))
    if any(t in chain for t in TOOL_TYPES):
        return ("Tool", 0, 0)
    if "ques" in chain or "body" in chain:
        return ("Quest", 0, 0)
    return None


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    excel = Path(sys.argv[1])
    itemtypes = read_table(excel / "itemtypes.txt")
    misc = read_table(excel / "misc.txt")

    equiv = {}
    for row in itemtypes:
        code = row.get("Code", "").strip()
        if code:
            equiv[code] = [e for e in (row.get("Equiv1", "").strip(), row.get("Equiv2", "").strip()) if e]

    def chain(t, seen=None):
        seen = set() if seen is None else seen
        if t in seen:
            return set()
        seen.add(t)
        out = {t}
        for e in equiv.get(t, []):
            out |= chain(e, seen)
        return out

    def pack(code):
        code = (code + "    ")[:4]
        return sum(ord(c) << (8 * i) for i, c in enumerate(code))

    entries = {}
    for row in misc:
        code = row.get("code", "").strip()
        base_type = row.get("type", "").strip()
        if not code or not base_type:
            continue
        result = classify(code, base_type, row.get("type2", "").strip(), chain(base_type))
        if result is None:
            continue
        try:
            w = max(1, int(row.get("invwidth", "1") or "1"))
            h = max(1, int(row.get("invheight", "1") or "1"))
        except ValueError:
            w, h = 1, 1
        entries[pack(code)] = (code, *result, w, h)

    lines = [
        "#pragma once",
        "",
        "// GENERATED FILE -- do not edit by hand.",
        "// Produced by tools/gen_misc_classification.py from a mod's",
        "// data/global/excel/{itemtypes,misc}.txt. See that script for the rules.",
        "//",
        f"// {len(entries)} misc item codes.",
        "",
        '#include "sort_engine.hpp"',
        "#include <cstdint>",
        "",
        "namespace Quyen194ItemSorting::Classification {",
        "",
        "struct MiscEntry {",
        "\tuint32_t code; // packed 4-byte item code, ascending, for binary search",
        "\tMiscKind kind;",
        "\tuint16_t order; // ItemFacts::miscOrder",
        "\tuint16_t grade; // ItemFacts::miscGrade",
        "\tuint8_t  width;",
        "\tuint8_t  height;",
        "};",
        "",
        f"inline constexpr MiscEntry kMiscTable[{len(entries)}] = {{",
    ]
    for packed in sorted(entries):
        code, kind, order, grade, w, h = entries[packed]
        lines.append(f"\t{{ 0x{packed:08X}U, MiscKind::{kind}, {order}u, {grade}u, {w}u, {h}u }}, // {code}")
    lines += ["};", "", "} // namespace Quyen194ItemSorting::Classification", ""]
    OUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"wrote {OUT} with {len(entries)} entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
