#!/usr/bin/env python3
"""Add the Quyen194 Item Sorting buttons to a mod's stash and cube layouts.

The D2RLoader PluginSDK can only attach a plugin child layout to
StockPanel::PlayerInventory, and a plugin's companion MPQ may not override
native layouts. The active mod's own MPQ *can* (resource order: active mod ->
plugin -> base game), so this script inserts small button strips into the
mod's own layout files:

  data/global/ui/layouts/bankexpansionlayouthd.json
    - SharedStashTabContainer  Auto Sort / Add Page / Remove Page. That
                               container is only shown while the Shared tab
                               is selected, so these hide on the other tabs.
    - basicstash_container     Sort Personal Stash. This container is shown
                               on both the Personal and the Shared tab (they
                               share one grid widget), and no widget exists
                               that only the Personal tab shows. So the
                               button sits exactly UNDER the shared-stash
                               Auto Sort button (same rect, inserted before
                               SharedStashTabContainer, which draws on top):
                               on the Shared tab Auto Sort covers it, on the
                               Personal tab it is the only button there.
    - advancedstash_horadriccube
                               Sort Cube, on the stash dialog's cube tab
                               (Reimagined). Skipped if the mod has no such
                               container.
  data/global/ui/layouts/horadriccubelayouthd.json
    - HoradricCubeLayout       Sort Cube, in the normal Horadric Cube panel.

(The inventory's Sort button needs no patch: the plugin adds it itself.)

The buttons are icon sprites shipped in the plugin's companion MPQ
(d2rloader/quyen194-item-sorting/stash_btn_*.sprite). Each button fires
"PanelManager:TogglePanel:quyen194-item-sorting/Action<Name>", which the plugin
turns into the matching action (see the g_action*Panel comment in plugin.cpp).

Usage:
    python patch_mod_stash_layout.py <mod.mpq folder>             # patch (or upgrade)
    python patch_mod_stash_layout.py <mod.mpq folder> --check     # report only
    python patch_mod_stash_layout.py <mod.mpq folder> --remove    # take every strip out again
    python patch_mod_stash_layout.py <mod.mpq folder> --controller  # also patch the controller/ layouts

Before any file is written it is copied to <file>.bak, or .bak1, .bak2, ... --
the first name that does not exist yet. Existing backups are never overwritten.

Re-run it after every mod update: the mod's installer replaces the layout
files, which drops the strips. Running it on an already-patched file is a
no-op.

Exit codes: 0 ok / already patched, 1 error, 2 (--check only) something not
patched or patched with an older strip version.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

# Sprites ship in the plugin's companion MPQ (tools/gen_stash_button_sprites.py,
# whose --size must match BUTTON_SIZE); frame order matches the native page
# arrows: normal, hovered, pressed, disabled.
SPRITE_DIR = "d2rloader/quyen194-item-sorting"
BUTTON_SIZE, BUTTON_GAP = 88, 8


@dataclass
class Strip:
    """One row of icon buttons inserted into one container of one layout file."""

    key: str                      # short id for messages
    marker_prefix: str            # strip widget name = marker_prefix + version
    version: int
    container: str                # name of the widget whose children get the strip
    position: str                 # "first" or "last" child of that container
    x: int                        # strip rect, in the container's coordinate space
    y: int
    buttons: list                 # (widget name, sprite, tooltip key, action panel)
    legacy_prefixes: tuple = ()   # older marker names still recognised for upgrade/removal
    required: bool = True         # False: skip quietly if the container is missing
    must_keep: tuple = field(default_factory=tuple)  # native widgets that must survive the edit

    @property
    def marker(self) -> str:
        return f"{self.marker_prefix}{self.version}"

    @property
    def all_prefixes(self) -> tuple:
        return (self.marker_prefix,) + self.legacy_prefixes

    @property
    def rect(self):
        n = len(self.buttons)
        return (self.x, self.y, n * BUTTON_SIZE + (n - 1) * BUTTON_GAP, BUTTON_SIZE)


# Shared-stash strip: three 88x88 icon buttons on the dark recessed strip left
# of the native page navigation (StashNavigation sits at x=668..1088,
# y=1506..1599; the 16x13 grid starts at x=90 and ends above y=1506),
# vertically centred on it. Coordinates are BankExpansionLayout-local, same
# space as StashNavigation. Strips written before the plugin was renamed from
# remote-stash-sorting to quyen194-item-sorting (strip v1/v2) are still
# recognised so they get upgraded or removed instead of being left behind.
SHARED_STRIP = Strip(
    key="shared-stash",
    marker_prefix="Quyen194ItemSortingStashButtons_v",
    version=4,
    legacy_prefixes=("Remote" + "StashSortingStashButtons_v",),
    container="SharedStashTabContainer",
    position="first",
    x=110,
    y=1508,
    buttons=[
        ("QisAutoSortButton", "stash_btn_autosort", "AutoSortTooltip", "ActionAutoSort"),
        ("QisAddBlankPageButton", "stash_btn_addpage", "AddBlankPageTooltip", "ActionAddBlankPage"),
        ("QisRemovePageButton", "stash_btn_removepage", "RemoveCurrentPageTooltip", "ActionRemoveCurrentPage"),
    ],
    must_keep=("StashNavigation",),
)

# Personal-stash Sort: same rect as the shared strip's Auto Sort button and
# inserted as the FIRST child of basicstash_container, i.e. before
# SharedStashTabContainer, which therefore draws on top of it. basicstash_
# container is visible on both the Personal and the Shared tab and D2R has no
# Personal-only container, so this is how the button disappears on the Shared
# tab (covered by Auto Sort). v1 sat right of the shared strip (x=398) as the
# last child and stayed visible on the Shared tab. If one click ever reaches
# both buttons, the plugin drops the personal sort (see DoSortGrid).
PERSONAL_STRIP = Strip(
    key="personal-stash",
    marker_prefix="Quyen194ItemSortingPersonalButtons_v",
    version=2,
    container="basicstash_container",
    position="first",
    x=SHARED_STRIP.x,
    y=SHARED_STRIP.y,
    buttons=[("QisSortPersonalStashButton", "stash_btn_autosort", "SortPersonalStashTooltip", "ActionSortPersonalStash")],
    must_keep=("grid",),
)

# Cube tab of the stash dialog (Reimagined): under the 12x8 cube grid (x=284,
# y=708..1492), left of the native Transmute button (x=810, y=1564).
CUBE_TAB_STRIP = Strip(
    key="stash-cube-tab",
    marker_prefix="Quyen194ItemSortingCubeTabButtons_v",
    version=1,
    container="advancedstash_horadriccube",
    position="last",
    x=284,
    y=1540,
    buttons=[("QisSortCubeTabButton", "stash_btn_autosort", "SortCubeTooltip", "ActionSortCube")],
    required=False,
    must_keep=("horadriccube_grid",),
)

# Normal Horadric Cube panel: under the grid (x=120, y=232..1016 for 12x8),
# on the same row as the native Transmute button (x=1174, y=1046).
CUBE_PANEL_STRIP = Strip(
    key="cube-panel",
    marker_prefix="Quyen194ItemSortingCubeButtons_v",
    version=1,
    container="HoradricCubeLayout",
    position="last",
    x=120,
    y=1046,
    buttons=[("QisSortCubeButton", "stash_btn_autosort", "SortCubeTooltip", "ActionSortCube")],
    must_keep=("grid",),
)

LAYOUT_FILES = {
    # key: (relative path, strips, controller variant?)
    "bank": (Path("data/global/ui/layouts/bankexpansionlayouthd.json"), [SHARED_STRIP, PERSONAL_STRIP, CUBE_TAB_STRIP], False),
    "cube": (Path("data/global/ui/layouts/horadriccubelayouthd.json"), [CUBE_PANEL_STRIP], False),
    "bank-controller": (Path("data/global/ui/layouts/controller/bankexpansionlayouthd.json"), [SHARED_STRIP, PERSONAL_STRIP, CUBE_TAB_STRIP], True),
    "cube-controller": (Path("data/global/ui/layouts/controller/horadriccubelayouthd.json"), [CUBE_PANEL_STRIP], True),
}


def build_block(strip: Strip, indent: str, nl: str) -> str:
    """The strip as JSON text, indented to sit inside a children array."""
    i1, i2, i3, i4 = indent + "    ", indent + "        ", indent + "            ", indent + "                "
    x, y, w, h = strip.rect
    lines = [
        f"{indent}{{",
        f'{i1}"type": "Widget", "name": "{strip.marker}",',
        f'{i1}"fields": {{',
        f'{i2}"rect": {{ "x": {x}, "y": {y}, "width": {w}, "height": {h} }}',
        f"{i1}}},",
        f'{i1}"children": [',
    ]
    for idx, (name, sprite, tooltip, action) in enumerate(strip.buttons):
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
            f"{i2}}}" + ("," if idx < len(strip.buttons) - 1 else ""),
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


def find_existing_strip(text: str, strip: Strip):
    alternatives = "|".join(re.escape(p) for p in strip.all_prefixes)
    m = re.search(r'"name"\s*:\s*"(?:' + alternatives + r')(\d+)"', text)
    if not m:
        return None
    start = enclosing_object_start(text, m.start())
    end = matching_close(text, start)
    return start, end, int(m.group(1))


def remove_strip(text: str, strip: Strip) -> str:
    found = find_existing_strip(text, strip)
    if not found:
        return text
    start, end, _ = found
    after = end + 1
    m = re.match(r"\s*,", text[after:])
    if m:
        # Inserted as the first child (or anywhere but last): drop the block
        # and the comma that follows it.
        after += m.end()
    else:
        # Inserted as the last child: drop the comma that precedes it.
        k = start - 1
        while k >= 0 and text[k] in " \t\r\n":
            k -= 1
        if k >= 0 and text[k] == ",":
            start = k
    line_start = text.rfind("\n", 0, start) + 1
    if text[line_start:start].strip() == "":
        start = line_start
        # keep the newline structure of what follows
        m2 = re.match(r"[ \t]*\r?\n", text[after:])
        if m2:
            after += m2.end()
    return text[:start] + text[after:]


def container_children(text: str, container: str):
    """(index of the children '[', index of its ']', object start) for the named widget, or None."""
    m = re.search(r'"name"\s*:\s*"' + re.escape(container) + '"', text)
    if not m:
        return None
    obj_start = enclosing_object_start(text, m.start())
    obj_end = matching_close(text, obj_start)
    ch = re.compile(r'"children"\s*:\s*\[').search(text, m.end(), obj_end)
    if not ch:
        raise ValueError(f'"{container}" has no children array')
    bracket = ch.end() - 1
    return bracket, matching_close(text, bracket), obj_start


def insert_strip(text: str, strip: Strip, nl: str) -> str:
    found = container_children(text, strip.container)
    if found is None:
        raise ValueError(f'widget "{strip.container}" not found -- the mod layout changed; the script needs updating')
    bracket, close, obj_start = found
    # indent = indentation of the first existing child, else container + 4
    im = re.match(r"\s*?\r?\n([ \t]*)\S", text[bracket + 1:])
    if im:
        indent = im.group(1)
    else:
        line_start = text.rfind("\n", 0, obj_start) + 1
        indent = text[line_start:obj_start] + "    "
    block = build_block(strip, indent, nl)
    if strip.position == "first":
        return text[: bracket + 1] + nl + block + "," + text[bracket + 1:]
    # last: after the final existing child
    k = close - 1
    while k > bracket and text[k] in " \t\r\n":
        k -= 1
    if k == bracket:  # empty children array
        return text[: bracket + 1] + nl + block + text[bracket + 1:]
    return text[: k + 1] + "," + nl + block + text[k + 1:]


# --- validation -------------------------------------------------------------

def lenient_json_loads(text: str):
    """json.loads that also accepts what D2R's own layout parser tolerates and
    some shipped layouts use: trailing commas and // or /* */ comments."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = skip_string(text, i)
            out.append(text[i:j])
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == ",":
            k = i + 1
            while k < n and text[k] in " \t\r\n":
                k += 1
            if k < n and text[k] in "]}":
                i += 1
                continue
        out.append(c)
        i += 1
    return json.loads("".join(out))


def find_widget(node, name):
    if isinstance(node, dict):
        if node.get("name") == name:
            return node
        for child in node.get("children", []) or []:
            r = find_widget(child, name)
            if r is not None:
                return r
    return None


def validate(text: str, strip: Strip, expect_patched: bool) -> None:
    doc = lenient_json_loads(text)
    container = find_widget(doc, strip.container)
    if container is None:
        raise ValueError(f"validation: {strip.container} missing after edit")
    names = [c.get("name") for c in container.get("children", [])]
    if expect_patched and strip.marker not in names:
        raise ValueError(f"validation: {strip.key} strip did not land inside {strip.container}")
    if any(str(n).startswith(strip.all_prefixes) and n != strip.marker for n in names):
        raise ValueError(f"validation: an older {strip.key} strip is still present")
    if not expect_patched and any(str(n).startswith(strip.all_prefixes) for n in names):
        raise ValueError(f"validation: {strip.key} strip still present after removal")
    for native in strip.must_keep:
        if find_widget(doc, native) is None:
            raise ValueError(f"validation: native {native} disappeared")


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


def status_of(text: str, strip: Strip) -> str:
    if container_children(text, strip.container) is None:
        return "no container"
    found = find_existing_strip(text, strip)
    if not found:
        return "not patched"
    return "patched" if found[2] == strip.version else f"patched with older strip v{found[2]}"


def process(path: Path, strips: list, mode: str) -> int:
    text, bom, nl = read_layout(path)
    original = text
    worst = 0
    changes = []
    print(f"{path}:")
    for strip in strips:
        status = status_of(text, strip)
        print(f"  {strip.key} ({strip.container}): {status}")
        if status == "no container":
            if strip.required and mode != "remove":
                print(f"    ERROR: widget \"{strip.container}\" not found -- the mod layout changed; the script needs updating", file=sys.stderr)
                worst = max(worst, 1)
            continue

        if mode == "check":
            if status != "patched":
                worst = max(worst, 2)
            continue

        if mode == "remove":
            if status == "not patched":
                continue
            text = remove_strip(text, strip)
            validate(text, strip, expect_patched=False)
            changes.append(f"{strip.key} removed")
            continue

        # patch / upgrade
        if status == "patched":
            continue
        base = remove_strip(text, strip) if status != "not patched" else text
        text = insert_strip(base, strip, nl)
        validate(text, strip, expect_patched=True)
        changes.append(f"{strip.key} {'upgraded' if status != 'not patched' else 'patched'}")

    if text != original:
        backup = write_layout(path, text, bom)
        print(f"  {', '.join(changes)}. backup: {backup.name}")
    elif mode == "patch" and worst == 0:
        print("  already patched -- no change.")
    elif mode == "remove":
        print("  nothing to remove.")
    return worst


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mod_mpq", type=Path, help="the mod's .mpq folder, e.g. ...\\mods\\Reimagined\\Reimagined.mpq")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--check", action="store_true", help="only report whether each layout is patched")
    g.add_argument("--remove", action="store_true", help="remove every button strip")
    ap.add_argument("--controller", action="store_true", help="also process the controller/ layouts")
    args = ap.parse_args()

    mode = "check" if args.check else "remove" if args.remove else "patch"

    worst = 0
    for key, (rel, strips, is_controller) in LAYOUT_FILES.items():
        if is_controller and not args.controller:
            continue
        path = args.mod_mpq / rel
        if not path.is_file():
            print(f"{path}: not found", file=sys.stderr)
            worst = max(worst, 1)
            continue
        try:
            worst = max(worst, process(path, strips, mode))
        except (ValueError, json.JSONDecodeError) as exc:
            print(f"{path}: ERROR: {exc} (file left unchanged)", file=sys.stderr)
            worst = max(worst, 1)
    return worst


if __name__ == "__main__":
    sys.exit(main())
