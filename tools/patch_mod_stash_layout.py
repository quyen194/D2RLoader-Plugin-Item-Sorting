#!/usr/bin/env python3
"""Add the Quyen194 Item Sorting buttons to a mod's shared-stash dialog.

The D2RLoader PluginSDK can only attach a plugin child layout to
StockPanel::PlayerInventory, and a plugin's companion MPQ may not override
native layouts. The active mod's own MPQ *can* (resource order: active mod ->
plugin -> base game), so this script inserts a small button strip into the
mod's data/global/ui/layouts/bankexpansionlayouthd.json, inside
BankExpansionLayout > basicstash_container > SharedStashTabContainer. That
container is only shown while the Shared tab is selected, so the buttons hide
themselves on the Personal / Gems / Materials / Runes tabs.

The buttons are icon sprites shipped in the plugin's companion MPQ
(d2rloader/quyen194-item-sorting/stash_btn_*.sprite). Each button fires
"PanelManager:TogglePanel:quyen194-item-sorting/Action<Name>", which the plugin
turns into the matching action (see the g_action*Panel comment in plugin.cpp).

Usage:
    python patch_mod_stash_layout.py <mod.mpq folder>             # patch (or upgrade)
    python patch_mod_stash_layout.py <mod.mpq folder> --check     # report only
    python patch_mod_stash_layout.py <mod.mpq folder> --remove    # take the strip out again
    python patch_mod_stash_layout.py <mod.mpq folder> --controller  # also patch controller/ layout

Before any file is written it is copied to <file>.bak, or .bak1, .bak2, ... --
the first name that does not exist yet. Existing backups are never overwritten.

Re-run it after every mod update: the mod's installer replaces the layout
file, which drops the strip. Running it on an already-patched file is a no-op.

Exit codes: 0 ok / already patched, 1 error, 2 (--check only) not patched or
patched with an older strip version.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

STRIP_VERSION = 4
MARKER_PREFIX = "Quyen194ItemSortingStashButtons_v"
# Strips written before the plugin was renamed from remote-stash-sorting to
# quyen194-item-sorting (strip v1/v2). Still recognised so they get upgraded
# or removed instead of being left behind next to a new strip.
LEGACY_MARKER_PREFIXES = ("Remote" + "StashSortingStashButtons_v",)
ALL_MARKER_PREFIXES = (MARKER_PREFIX,) + LEGACY_MARKER_PREFIXES
MARKER = f"{MARKER_PREFIX}{STRIP_VERSION}"

LAYOUT_FILES = {
    "mouse": Path("data/global/ui/layouts/bankexpansionlayouthd.json"),
    "controller": Path("data/global/ui/layouts/controller/bankexpansionlayouthd.json"),
}

# Three 88x88 icon buttons on the dark recessed strip left of the native page
# navigation (StashNavigation sits at x=668..1088, y=1506..1599; the 16x13
# grid starts at x=90 and ends above y=1506), vertically centred on it.
# Coordinates are BankExpansionLayout-local, same space as StashNavigation.
# Sprites ship in the plugin's companion MPQ (tools/gen_stash_button_sprites.py,
# whose --size must match BUTTON_SIZE);
# frame order matches the native page arrows: normal, hovered, pressed, disabled.
SPRITE_DIR = "d2rloader/quyen194-item-sorting"
BUTTONS = [
    # (widget name, sprite, tooltip key, action panel)
    ("QisAutoSortButton", "stash_btn_autosort", "AutoSortTooltip", "ActionAutoSort"),
    ("QisAddBlankPageButton", "stash_btn_addpage", "AddBlankPageTooltip", "ActionAddBlankPage"),
    ("QisRemovePageButton", "stash_btn_removepage", "RemoveCurrentPageTooltip", "ActionRemoveCurrentPage"),
]
BUTTON_SIZE, BUTTON_GAP = 88, 8
STRIP_X, STRIP_Y = 110, 1508
STRIP_RECT = (STRIP_X, STRIP_Y, len(BUTTONS) * BUTTON_SIZE + (len(BUTTONS) - 1) * BUTTON_GAP, BUTTON_SIZE)


def build_block(indent: str, nl: str) -> str:
    """The strip as JSON text, indented to sit inside a children array."""
    i1, i2, i3, i4 = indent + "    ", indent + "        ", indent + "            ", indent + "                "
    x, y, w, h = STRIP_RECT
    lines = [
        f"{indent}{{",
        f'{i1}"type": "Widget", "name": "{MARKER}",',
        f'{i1}"fields": {{',
        f'{i2}"rect": {{ "x": {x}, "y": {y}, "width": {w}, "height": {h} }}',
        f"{i1}}},",
        f'{i1}"children": [',
    ]
    for idx, (name, sprite, tooltip, action) in enumerate(BUTTONS):
        bx = idx * (BUTTON_SIZE + BUTTON_GAP)
        lines += [
            f"{i2}{{",
            f'{i3}"type": "ButtonWidget", "name": "{name}",',
            f'{i3}"fields": {{',
            f'{i4}"rect": {{ "x": {bx}, "y": 0, "width": {BUTTON_SIZE}, "height": {BUTTON_SIZE} }},',
            f'{i4}"filename": "{SPRITE_DIR}/{sprite}",',
            f'{i4}"normalFrame": 0,',
            f'{i4}"hoveredFrame": 1,',
            f'{i4}"pressedFrame": 2,',
            f'{i4}"disabledFrame": 3,',
            f'{i4}"tooltipString": "@quyen194-item-sorting:{tooltip}",',
            f'{i4}"onClickMessage": "PanelManager:TogglePanel:quyen194-item-sorting/{action}"',
            f"{i3}}}",
            f"{i2}}}" + ("," if idx < len(BUTTONS) - 1 else ""),
        ]
    lines += [f"{i1}]", f"{indent}}}"]
    return nl.join(lines)


# --- text helpers -----------------------------------------------------------

def skip_string(text: str, pos: int) -> int:
    """pos is at an opening quote; return the index just past the closing one."""
    pos += 1
    while pos < len(text):
        c = text[pos]
        if c == "\\":
            pos += 2
            continue
        if c == '"':
            return pos + 1
        pos += 1
    raise ValueError("unterminated string")


def matching_close(text: str, open_pos: int) -> int:
    """Index of the bracket closing the one at open_pos ({ or [)."""
    depth = 0
    pos = open_pos
    while pos < len(text):
        c = text[pos]
        if c == '"':
            pos = skip_string(text, pos)
            continue
        if c in "{[":
            depth += 1
        elif c in "}]":
            depth -= 1
            if depth == 0:
                return pos
        pos += 1
    raise ValueError("unbalanced brackets")


def enclosing_object_start(text: str, pos: int) -> int:
    """Index of the '{' of the innermost object containing pos."""
    depth = 0
    i = pos
    # Walk backwards; strings could contain braces, but the marker/name lines
    # we search from sit directly inside their own object, and none of the
    # native strings between contain unbalanced braces. Validated afterwards
    # by a full JSON parse.
    while i >= 0:
        c = text[i]
        if c == "}":
            depth += 1
        elif c == "{":
            if depth == 0:
                return i
            depth -= 1
        i -= 1
    raise ValueError("no enclosing object")


def find_existing_strip(text: str):
    alternatives = "|".join(re.escape(p) for p in ALL_MARKER_PREFIXES)
    m = re.search(r'"name"\s*:\s*"(?:' + alternatives + r')(\d+)"', text)
    if not m:
        return None
    start = enclosing_object_start(text, m.start())
    end = matching_close(text, start)
    return start, end, int(m.group(1))


def remove_strip(text: str) -> str:
    found = find_existing_strip(text)
    if not found:
        return text
    start, end, _ = found
    # Drop the block, the comma that followed it (it's always inserted first
    # in the array, followed by ","), and the line it sat on.
    after = end + 1
    m = re.match(r"\s*,", text[after:])
    if m:
        after += m.end()
    line_start = text.rfind("\n", 0, start) + 1
    if text[line_start:start].strip() == "":
        start = line_start
        # keep the newline structure of what follows
        m2 = re.match(r"[ \t]*\r?\n", text[after:])
        if m2:
            after += m2.end()
    return text[:start] + text[after:]


def insert_strip(text: str, nl: str) -> str:
    m = re.search(r'"name"\s*:\s*"SharedStashTabContainer"', text)
    if not m:
        raise ValueError('widget "SharedStashTabContainer" not found -- the mod layout changed; the script needs updating')
    obj_start = enclosing_object_start(text, m.start())
    obj_end = matching_close(text, obj_start)
    ch = re.compile(r'"children"\s*:\s*\[').search(text, m.end(), obj_end)
    if not ch:
        raise ValueError('"SharedStashTabContainer" has no children array')
    bracket = ch.end() - 1
    # indent = indentation of the first existing child, else container + 4
    after = text[bracket + 1:]
    im = re.match(r"\s*?\r?\n([ \t]*)\S", after)
    if im:
        indent = im.group(1)
    else:
        line_start = text.rfind("\n", 0, obj_start) + 1
        indent = text[line_start:obj_start] + "    "
    block = build_block(indent, nl)
    return text[: bracket + 1] + nl + block + "," + text[bracket + 1:]


# --- validation -------------------------------------------------------------

def find_widget(node, name):
    if isinstance(node, dict):
        if node.get("name") == name:
            return node
        for child in node.get("children", []) or []:
            r = find_widget(child, name)
            if r is not None:
                return r
    return None


def validate(text: str, expect_patched: bool) -> None:
    doc = json.loads(text)
    container = find_widget(doc, "SharedStashTabContainer")
    if container is None:
        raise ValueError("validation: SharedStashTabContainer missing after edit")
    names = [c.get("name") for c in container.get("children", [])]
    if expect_patched and MARKER not in names:
        raise ValueError("validation: strip did not land inside SharedStashTabContainer")
    if any(str(n).startswith(ALL_MARKER_PREFIXES) and n != MARKER for n in names):
        raise ValueError("validation: an older strip is still present")
    if not expect_patched and any(str(n).startswith(ALL_MARKER_PREFIXES) for n in names):
        raise ValueError("validation: strip still present after removal")
    if find_widget(doc, "StashNavigation") is None:
        raise ValueError("validation: native StashNavigation disappeared")


# --- file handling ----------------------------------------------------------

def next_backup_path(path: Path) -> Path:
    candidate = path.with_name(path.name + ".bak")
    n = 1
    while candidate.exists():
        candidate = path.with_name(f"{path.name}.bak{n}")
        n += 1
    return candidate


def read_layout(path: Path):
    raw = path.read_bytes()
    bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig")
    nl = "\r\n" if "\r\n" in text else "\n"
    return text, bom, nl


def write_layout(path: Path, text: str, bom: bool) -> Path:
    backup = next_backup_path(path)
    shutil.copy2(path, backup)
    data = text.encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    path.write_bytes(data)
    return backup


def status_of(text: str) -> str:
    found = find_existing_strip(text)
    if not found:
        return "not patched"
    return "patched" if found[2] == STRIP_VERSION else f"patched with older strip v{found[2]}"


def process(path: Path, mode: str) -> int:
    text, bom, nl = read_layout(path)
    status = status_of(text)
    print(f"{path}: {status}")

    if mode == "check":
        return 0 if status == "patched" else 2

    if mode == "remove":
        if status == "not patched":
            print("  nothing to remove.")
            return 0
        new = remove_strip(text)
        validate(new, expect_patched=False)
        backup = write_layout(path, new, bom)
        print(f"  removed. backup: {backup.name}")
        return 0

    # patch / upgrade
    if status == "patched":
        print("  already patched -- no change.")
        return 0
    base = remove_strip(text) if status != "not patched" else text
    new = insert_strip(base, nl)
    validate(new, expect_patched=True)
    backup = write_layout(path, new, bom)
    print(f"  {'upgraded' if status != 'not patched' else 'patched'}. backup: {backup.name}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mod_mpq", type=Path, help="the mod's .mpq folder, e.g. ...\\mods\\Reimagined\\Reimagined.mpq")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--check", action="store_true", help="only report whether each layout is patched")
    g.add_argument("--remove", action="store_true", help="remove the button strip")
    ap.add_argument("--controller", action="store_true", help="also process controller/bankexpansionlayouthd.json")
    args = ap.parse_args()

    mode = "check" if args.check else "remove" if args.remove else "patch"
    targets = ["mouse"] + (["controller"] if args.controller else [])

    worst = 0
    for key in targets:
        path = args.mod_mpq / LAYOUT_FILES[key]
        if not path.is_file():
            print(f"{path}: not found", file=sys.stderr)
            worst = max(worst, 1)
            continue
        try:
            worst = max(worst, process(path, mode))
        except (ValueError, json.JSONDecodeError) as exc:
            print(f"{path}: ERROR: {exc} (file left unchanged)", file=sys.stderr)
            worst = max(worst, 1)
    return worst


if __name__ == "__main__":
    sys.exit(main())
