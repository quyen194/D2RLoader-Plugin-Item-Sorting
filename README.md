# Quyen19492 Item Sorting

A D2RLoader plugin for Diablo II: Resurrected with the **Reimagined** mod. It
brings the shared-stash **Auto Sort**, **Add Page** and **Remove Page** features
of [D2R-Reimagined-Save-Editor](../D2R-Reimagined-Save-Editor) into the live
game, and adds a **Sort** button for the personal stash, the inventory and the
Horadric Cube.

Everything goes through the official [D2RLoader Plugin SDK](../PluginSDK)
services (`InventoryService`, `ItemService`, `PanelService`, `ResourceService`,
`WidgetService`, `SharedEventService`, `ThreadService`). The plugin uses no
native hooks and no raw game pointers; where the SDK has no API for something,
it uses a workaround (see [Known limitations](#known-limitations)).

End-user install instructions are in [packaging/README.md](packaging/README.md).
This file is for developers.

## Features

- **Auto Sort** (shared stash): sorts the whole shared stash by category
  (set, unique/rare/magic/superior/normal gear, charms, jewels, rings/amulets,
  runes, gems, potions, ..., plus an `unclassified` catch-all), using the
  save-editor's category/quota layout. Optional padding: blank pages after a
  category, blank pages at the start, and compacting empty pages first.
- **Add Page**: moves the nearest empty page to sit right after the page on
  screen. (The SDK cannot create pages; this shifts page contents with item
  moves.)
- **Remove Page**: pushes the page on screen, which must be empty, to the end
  of the stash.
- **Sort** for the personal stash, the inventory and the Horadric Cube: arranges
  one grid by category, with zones, corner anchors, locked cells and
  keep-in-place categories, configured separately for each container.
- Results appear in the plugin's own toast panel (`notification_enabled =
  false` turns it off); details go to the plugin log.

## Source layout

```text
src/sort_engine.*            Shared-stash Auto Sort / Add / Remove Page planner (pure, no SDK)
src/item_classification.*    Item lookup by packed code; *_data.hpp is GENERATED
src/item_misc_data.hpp       GENERATED: rune/gem/potion/tool/quest kinds from misc.txt
src/mod_item_data.*          Runtime parse of the mod's own txt files (mod_data_path)
src/grid_sort.*              Single-grid sort (personal stash / inventory / cube), pure
src/grid_config_toml.hpp     TOML -> Grid::Config
src/ui_layouts.hpp           Toast widget names shared with the mpq's Toasthd.json
src/plugin.cpp               SDK entry points, service wiring, thread hops, UI listener
src/plugin_version.h.in      -> plugin_version.h (VERSIONINFO in plugin.rc + PluginInfo)
d2rl-quyen19492-item-sorting.mpq/   Companion resources: layouts, sprites, strings
quyen19492-item-sorting.toml Default config (embedded in the DLL)
tools/                       Generators, patch_mod_stash_layout.py, set_version.py
tests/grid_sort_test.cpp     Engine / config / classification tests (no SDK needed)
packaging/README.md          End-user install guide
```

## How it works

### Buttons and messages

D2R only delivers a button's `onClickMessage` to message channels it already
knows, and a plugin id is not one of them. So each button sends the native
`PanelManager:TogglePanel:quyen19492-item-sorting/Action<Name>` against an
invisible, empty trigger panel (`Action<Name>hd.json` in the companion MPQ).
One `UiMessageListener` in `plugin.cpp` matches `PanelManager` / `TogglePanel`,
closes the trigger panel again and runs the matching action:

| Trigger panel | Action |
| --- | --- |
| `ActionAutoSort` | Auto Sort |
| `ActionAddBlankPage` / `ActionRemoveCurrentPage` | Add / Remove Page |
| `ActionSortPersonalStash` / `ActionSortInventory` / `ActionSortCube` | single-grid Sort |

The toast's Close button sends `PanelManager:ClosePanel:quyen19492-item-sorting/Toast`
directly and needs no handler.

Where the buttons are:

- **Inventory Sort**: the plugin attaches child layout `InventoryTools` to
  `StockPanel::PlayerInventory` (the only panel the SDK lets plugins attach to).
- **Everything else** (Auto Sort / Add Page / Remove Page in the shared-stash
  dialog, personal-stash Sort, both Cube Sort buttons): a companion MPQ cannot
  override native layouts, but the mod's own MPQ can, so
  `tools/patch_mod_stash_layout.py` inserts the buttons into the mod's layout
  files (see [below](#patching-the-mod-layouts)).

Add Page / Remove Page can be hidden with `add_page_enabled` /
`remove_page_enabled` (and all three shared-stash buttons with
`[item-sorting.shared_stash] enabled = false`). The plugin hides them with
`WidgetService::setWidgetVisible` whenever the stash opens or a stash tab is
selected.

### Threading

Item moves (`ItemService::executeExistingItemTransaction`) run on the game
thread; widget and panel calls run on the UI thread. A click arrives on the UI
thread, hops to the game thread to plan and apply the moves, then back to the
UI thread to show the toast.

### Auto Sort pipeline

1. Scan every shared-stash item (`forEachInventoryItem`) and classify it from
   its packed item code (embedded table, or the mod's own txt files when
   `mod_data_path` is set). Items with no page number (Reimagined's extra
   Gems / Materials / Runes tabs, which the SDK will not move) are left out.
2. `RunAutoSortWithPadding` (`sort_engine.cpp`): optionally compact empty
   pages, then place each category in layout order starting at `start_tab`.
   A category reserves `min_tabs` pages and borrows more if it needs them.
   Items are packed best-fit within a page; set pieces stay together. Then
   add the configured blank pages. The final plan has exactly one move per
   item handle, because a transaction rejects a chunk if any handle appears in
   it twice.
3. `DetectCollisions` checks that the final layout has no overlaps.
4. `SequenceMovesForLiveApply` orders the moves against the *current*
   occupancy: it splits them into layers where every move lands on cells that
   are already free, and breaks cycles (A wants B's cell, B wants A's) by
   parking one item in a free scratch cell. If a cycle cannot be broken, the
   whole plan is refused.
5. `ApplyMoves` sends each layer in transactions of up to 4096 operations. If
   a transaction fails, it is split in halves and retried, so one rejected
   item does not block the rest. Every rejection is logged with the item code,
   category, size, source and destination, followed by a per-page summary.

Add Page / Remove Page use the same steps 3-5. The page on screen is tracked
from game events (see [Known limitations](#known-limitations)).

### Single-grid sort (personal stash, inventory, cube)

`src/grid_sort.*`:

1. Items outside `grid_width` x `grid_height` are ignored (logged).
2. Items on `locked_cells`, matching `keep_in_place`, or matching no layout
   entry (when there is no `unclassified` entry) stay put and act as obstacles.
3. Every other item goes to the first layout entry whose mode matches. Here
   the gear modes take only real gear (an item with a gear slot).
4. Entries with a `zone` are placed first, then the rest. Each entry sorts by
   its `sort_keys` and places first-fit from its `anchor` corner in
   `fill_direction` order; `category_break = "band"` starts the next entry
   with the same anchor in a fresh column/row.
5. If something does not fit, `on_overflow = "compact"` retries once without
   bands; otherwise nothing moves.
6. `Grid::SequenceForLiveApply` turns the plan into single-item steps that
   each land on cells no other item occupies at that moment, parking blockers
   on free cells. The sequence is simulated first; each step is its own
   one-operation transaction, stopping at the first rejection. Afterwards the
   grid is re-read and checked (`post-sort check` in the log).

The whole plan is not sent as one transaction because live testing showed
that moving an item onto a cell that another item of the same transaction
still occupies makes items disappear from the grid.

### Item classification

`src/item_classification_data.hpp` is generated from the save-editor's
`base/*.txt` (item types, weapons, armor, misc, set/unique items, magic
prefixes/suffixes) and gives each item its gear slot / kind, size, set group
and the true required level (base, unique/set row, or highest affix).
`src/item_misc_data.hpp` adds the rune / gem / potion / tool / quest kinds from
misc.txt. With `mod_data_path` set, `mod_item_data.cpp` parses the mod's own
txt files at runtime with the same rules instead, so a mod update that changes
row order does not break classification.

### Log

D2RLoader prefixes every line with `[Plugin:quyen19492-item-sorting]`. Each
message then starts with a tag: `[remote-stash]` (shared stash),
`[personal-stash]`, `[inventory]`, `[cube]`, `[config]` or `[plugin]`
(`LogTagFor()` in `plugin.cpp`). Shared-stash pages and grid cells are
**1-based** in the log (as in-game: page 1, cell (1,1) top-left); use
`ItemSorting::PageText` / `CellText` for new log text.
`debug_log_ui_messages = true` logs every UI message the plugin sees.

## Config (`quyen19492-item-sorting.toml`)

D2RLoader creates `d2rloader/config/quyen19492-item-sorting.toml` from the
embedded default on first load and never overwrites it. The file's own
comments list every key and value.

| Section | Holds | Read |
| --- | --- | --- |
| `[item-sorting]` | `enabled`, `notification_*`, `debug_log_ui_messages`, `mod_data_path` | at game start; `notification_enabled` also on every button press and whenever the stash opens |
| `[item-sorting.shared_stash]` + `[[...shared_stash.layout]]` | `enabled`, `add_page_enabled`, `remove_page_enabled`, `grid_width`/`grid_height`, `shared_stash_tab_count`, `start_tab`, `blank_tabs_at_start`, `compact_empty_tabs_first`, `add_empty_page_when`, the category list | on every shared-stash button press; `enabled` and the page toggles also whenever the stash opens |
| `[item-sorting.personal_stash]` / `.inventory` / `.cube` | single-grid settings and layout | on every Sort press |

So most changes take effect the next time a button is pressed, without
restarting the game. An invalid value makes that container refuse to sort,
with a toast and the exact problem in the log.

Settings worth knowing:

- `shared_stash_tab_count` must match the real number of shared-stash pages
  (the SDK cannot report it).
- Each `[[layout]]` entry is one category, in fill order. `mode_id` is one of
  `set`, `uniqueGear`, `rareGear`, `magicGear`, `superiorGear`, `normalGear`,
  `uniqueCharm`, `magicCharm`, `uniqueJewel`, `rareMagicJewel`,
  `uniqueRingAmu`, `rareRingAmu`, `magicRingAmu`, `rune`, `gem`, `potion`,
  `tool`, `quest`, `unclassified`. `unclassified` takes everything no other
  listed category claims, so it should go last.
- `sort_keys` / `gear_groups` per entry override the sort order and the gear
  slot order; both have built-in defaults.
- `mod_data_path`: empty uses the table built into the DLL; a mod folder (the
  one containing `data/global/excel`) makes the plugin parse that mod's txt
  files instead, falling back to the built-in table if they can't be read.

## Known limitations

1. **Shared-stash page count.** The SDK cannot report it; it comes from
   `shared_stash_tab_count`.
2. **Page on screen.** The SDK cannot report it. The plugin tracks it from
   events: hovering a shared-stash item sets it exactly
   (`ItemInfo::sharedStashPage`), the native page arrows
   (`BankPanelMessage:SharedStashRight/Left`) move it by one, and reopening the
   stash or pressing an arrow on the first/last page forgets it. Add Page /
   Remove Page refuse to run while it is unknown.
3. **No dynamic widget text.** `WidgetService` cannot set a widget's text, so
   the toast has one hidden `TextBoxWidget` per possible message and shows one.
   Exact counts are only in the log.
4. **Multiplayer client.** On a TCP/IP client, item transactions return
   `NotAuthoritative`; the plugin shows a "not supported as a client" toast.
   Relaying the plan to the host over `NetworkService` is required but not
   implemented yet.
5. **Very large plans.** One transaction holds at most 4096 operations; a
   larger layer is split into several transactions, which are not atomic as a
   whole (logged as a warning).
6. **Mod layout patch.** Buttons inside the stash and cube panels exist only
   after `tools/patch_mod_stash_layout.py` has patched the mod, and must be
   re-patched after every mod update.
7. **Not ported from the save-editor:** the `unidentified` mode (such items go
   to `unclassified`), redistribution of unused category quota, profiles, and
   sweeping foreign items out of a target page.
8. **Deliberate differences from the save-editor:** items are packed best-fit
   within a page (denser, but on-screen order can differ slightly from sort
   order), and Add Page / padding move the *nearest* empty page rather than
   the last one, because every page shift costs one move per item in between.

## Patching the mod layouts

```text
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq            # patch / upgrade
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq --check    # exit 0 current, 2 not patched / older
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq --remove   # remove
```

| Strip | File | Container |
| --- | --- | --- |
| Auto Sort / Add Page / Remove Page | `bankexpansionlayouthd.json` | `SharedStashTabContainer` (left of "Page N / 200") |
| Sort Personal Stash | `bankexpansionlayouthd.json` | `basicstash_container` (first child, under Auto Sort; Auto Sort covers it on the Shared tab) |
| Sort Cube (stash cube tab) | `bankexpansionlayouthd.json` | `advancedstash_horadriccube` (skipped if absent) |
| Sort Cube | `horadriccubelayouthd.json` | `HoradricCubeLayout` |

Each strip has a version marker (`Quyen19492ItemSorting*Buttons_v<N>`), so it
is checked, upgraded and removed independently. Every write first backs up the
file (`.bak`, `.bak1`, ...; never overwritten) and re-validates the JSON.
`--controller` also patches the `controller/` copies.

Button sprites come from `assets/stash_buttons/*.png`;
`tools/gen_stash_button_sprites.py [--size 88]` (Pillow) builds the 4-frame
`SpA1` sprites (+ `.lowend`) into the companion MPQ. Keep `--size` in sync with
`BUTTON_SIZE` in `patch_mod_stash_layout.py`.

## Building

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

- Windows, MSVC x64 (VS2022 or newer; VS2019 16.11 crashes on designated
  initializers), C++20, CMake 3.29+, **Ninja** generator. In VS Code's CMake
  Tools, select an MSVC kit and the Release variant.
- Play-test Release builds only: Debug is 20-70x slower at planning.
- Every build copies the outputs into `deploy/` (gitignored):
  `deploy/d2rloader/plugins` (`d2rl-quyen19492-item-sorting.dll` +
  companion `.mpq` folder), `deploy/d2rloader/config`, `deploy/README.md`
  (from `packaging/README.md`) and `deploy/patch_mod_stash_layout.py`.
  `deploy/d2rloader` can be dropped straight into the game folder.
- `-DD2RLOADER_GAME_DIR=<game>` also copies the DLL and companion folder into
  `<game>/d2rloader/plugins` on every build.
- Copying never deletes; remove stale files by hand.

### Version and releases

The version lives in `project(ItemSorting VERSION x.y.z)` in `CMakeLists.txt`
and ends up in the DLL's VERSIONINFO and `PluginInfo.version`. Change it with:

```text
python tools/set_version.py              # print the current version
python tools/set_version.py 1.2.3        # or: major | minor | patch
python tools/set_version.py patch --tag  # also commit + tag vX.Y.Z
```

Pushing a `v*` tag runs `.github/workflows/release.yml`: it checks out this
repo and [`D2RLoader/PluginSDK`](https://github.com/D2RLoader/PluginSDK) side
by side, builds Release/x64, and attaches a zip (`d2rloader/`, `README.md`,
`patch_mod_stash_layout.py`) plus `packaging/README.md` to the GitHub Release.
The tag must match the `CMakeLists.txt` version.

```text
python tools/set_version.py 1.0.1 --tag
git push origin main v1.0.1
```

## Testing

```text
cmake -S . -B build -G Ninja -DQIS_BUILD_TESTS=ON
cmake --build build
build/ItemSortTests [<game>/mods/Reimagined/Reimagined.mpq]
```

Run from the repo root; exit code 0 means pass. `tests/grid_sort_test.cpp`
checks that the shipped config parses and matches the built-in defaults,
config-error detection, classification (and, with the mod folder argument,
that `mod_data_path` parsing agrees with the generated tables), the toast
widget names, hand-checked placement scenarios, and 1,200 randomized grids
(no overlaps, locked/fixed items untouched, re-sorting is a no-op). The pure
engine also builds with g++/clang++ (see the file header).

Anything touching `plugin.cpp`, layouts or item transactions also needs a
live-game test.

## Regenerating item data

Only needed when the game or mod item data changes.

```text
python3 tools/gen_classification.py
python3 tools/gen_misc_classification.py "<game>/mods/Reimagined/Reimagined.mpq/data/global/excel"
```

The first reads `../D2R-Reimagined-Save-Editor/base/*.txt` and rewrites
`src/item_classification_data.hpp`; the second rewrites
`src/item_misc_data.hpp`. `src/mod_item_data.cpp` mirrors their rules for
`mod_data_path`; keep them in sync (the tests compare them when given the mod
folder).
