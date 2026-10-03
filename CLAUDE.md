# CLAUDE.md

Guidance for Claude when working in this repo. `README.md` is the full developer
documentation (design decisions, numbered "Known gaps", config reference); read the
relevant section there before changing behaviour it describes.

## What this is

`quyen19492-item-sorting` — a D2RLoader plugin (C++20, Windows x64 DLL) for
Diablo II: Resurrected, used with the **Reimagined** mod. It ports the shared-stash
auto-sort / add-blank-page / remove-current-page features of
D2R-Reimagined-Save-Editor into a live in-game plugin, and adds a **Sort** button for
the personal stash, inventory and Horadric Cube (single-grid sorting).

- Plugin id: `quyen19492-item-sorting` → `d2rl-quyen19492-item-sorting.dll`, companion
  `d2rl-quyen19492-item-sorting.mpq` (a folder in dev), config `quyen19492-item-sorting.toml`
  (sections `[item-sorting]`, `[shared_stash]`, per-grid sections).
- Formerly `remote-stash-sorting`, then `quyen194-item-sorting`; don't reintroduce
  the old names. User-visible names use `quyen19492-item-sorting`; C++ namespace and
  CMake targets are plain `ItemSorting` / `ItemSortEngine` / `ItemSortTests`.

## Hard constraints (owner's decisions)

- **Official PluginSDK services only.** Never use `native_hook`, `InstallInlineHook`,
  `hooks.h`, `mutations.h` or raw game pointers. Everything goes through
  `InventoryService`, `ItemService`, `PanelService`, `ResourceService`,
  `WidgetService`, `SharedEventService`, `ThreadService`. If something can't be done
  with the SDK, say so and propose a workaround — don't reach for a hook.
- **Triggered by UI buttons**, not console commands or hotkeys.
- **Must work as a multiplayer TCP/IP client**, not only host/local (see README Known
  gaps #9 for current status).
- Single-grid scope: also sorts the Cube; grid sizes are configurable; there is
  deliberately **no** "send inventory items to stash" button; charms are arranged by
  hand by the user.

## Layout

```
src/sort_engine.*            Shared-stash auto-sort (pure, no SDK dependency)
src/item_classification.*    Item lookup by packed code; *_data.hpp is GENERATED
src/item_misc_data.hpp       GENERATED (tools/gen_misc_classification.py)
src/mod_item_data.*          Runtime parse of the mod's own txt files (mod_data_path)
src/grid_sort.*              Single-grid sort (personal stash / inventory / cube), pure
src/grid_config_toml.hpp     TOML -> Grid::Config
src/ui_layouts.hpp           Toast widget names shared with the mpq Toasthd.json
src/plugin.cpp               SDK entry points, service wiring, thread hops, UI listener
d2rl-quyen19492-item-sorting.mpq/   Companion resources (layouts, sprites, strings)
src/plugin_version.h.in      -> generated plugin_version.h (VERSIONINFO in plugin.rc + PluginInfo)
tools/                       Generators, patch_mod_stash_layout.py, set_version.py (bump version)
tests/grid_sort_test.cpp     Engine/config/classification tests (no SDK needed)
packaging/README.md          End-user install guide (copied to deploy/README.md)
```

Generated headers are committed; edit the generator in `tools/` and regenerate rather
than hand-editing them.

## Sibling folders (paths relative to this repo)

- `../PluginSDK` — D2RLoader Plugin SDK source checkout, pulled in by
  `add_subdirectory`. Ground truth for APIs: `include/D2RLPlugin/*.h` and
  `examples/*`. Match the examples' call patterns.
- `../D2R-Reimagined-Save-Editor` — origin of the sort algorithm (`_ssRunAutoSort`,
  `_ssPrepareInPlaceSort`, ...) and `base/*.txt` for `gen_classification.py`.
- `../RuffnecKk-D2RLoader-Suite` — reference only; uses native hooks, don't copy
  that approach.
- Game: `D:/Games/DiabloII-Resurrected/DiabloII-Resurrected`; mod data:
  `mods/Reimagined/Reimagined.mpq` (a folder).

## Build

- Windows, MSVC x64 (VS2022 or newer — VS2019/16.11 ICEs on designated initializers),
  CMake ≥ 3.29, **Ninja** generator (not a VS solution generator). Run from a
  "x64 Native Tools" / dev shell so `cl` is on PATH.
- VS Code CMake Tools is configured in `.vscode/settings.json` (Ninja,
  `-DD2RLOADER_GAME_DIR=...`), so a build also copies the DLL + companion `.mpq`
  folder straight into the game's `d2rloader/plugins`.
- Command line:
  ```
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```
- Every build mirrors outputs into `deploy/` (gitignored): `deploy/d2rloader/plugins`,
  `deploy/d2rloader/config`, `README.md`, `patch_mod_stash_layout.py`.
  `copy_directory` never deletes — remove stale files from `deploy/` and the game
  folder by hand.
- Release: pushing a `v*` tag runs `.github/workflows/release.yml` (expects this repo
  checked out as `Quyen19492-Item-Sorting` next to `PluginSDK`).

## Test

- `cmake -S . -B build -G Ninja -DQIS_BUILD_TESTS=ON && cmake --build build`, then run
  `ItemSortTests` **from the repo root**. Optional argument: the mod `.mpq`
  folder for the `mod_data_path` parity check. Exit code 0 = pass.
- The pure engine also builds with plain g++/clang++ (see the test file header).
- Anything touching `plugin.cpp`, layouts or item transactions needs a **live-game
  test** by the owner — say what to check in-game rather than claiming it works.

## Gotchas already learned

- Item mutation (`ItemService::executeExistingItemTransaction`) only on the game thread
  (`ThreadService::runOnGameThread`); widget/panel calls only on the UI thread. Button
  clicks arrive on the UI thread → hop to game thread → back to UI for the toast.
- A transaction rejects the whole chunk if any item handle appears twice — plans must
  contain one move per handle (see `RunAutoSortWithPadding`). Chunks max 4096 ops.
- The SDK can't report the shared-stash tab count or which tab is on screen; both are
  worked around (README Known gaps #3, #4). No dynamic widget text (#5).
- UI messages: `"quyen19492-item-sorting:<Command>"`, one `UiMessageListener` in
  `plugin.cpp` dispatches on `command`.
- After changing button ids or layouts, users must re-run
  `tools/patch_mod_stash_layout.py` against the mod.

## Conventions

- Owner communicates in Vietnamese; code, comments, commits and docs are in English.
- Keep README.md in sync when behaviour, config keys or Known gaps change.
- Don't commit save files (`*.d2i`) or anything under `build/` / `deploy/`.
