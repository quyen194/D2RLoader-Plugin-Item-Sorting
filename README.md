# Remote Stash Sorting

A D2RLoader plugin that ports the shared-stash **auto-sort**, **add blank
page**, and **remove current page** features from
[D2R-Reimagined-Save-Editor](../D2R-Reimagined-Save-Editor) into a live,
in-game plugin, built entirely on the official [D2RLoader Plugin
SDK](../PluginSDK) services. **It does not use `native_hook` /
`InstallInlineHook` / `hooks.h` / `mutations.h` anywhere** -- every feature
here (reading items, moving items, adding UI, catching button clicks) goes
through an officially supported service call. See "Why no native hooks"
below for why that constraint shaped several design decisions.

## Status

This is a first, unreviewed pass at Phase 2 (the plugin itself). The pure
sort algorithm (Phase 1, `src/sort_engine.*` + `src/item_classification.*`)
compiles clean and has been smoke-tested end to end on Linux (see
"Testing" below). `src/plugin.cpp` -- the actual D2RLoader integration --
has been written carefully against the SDK headers and the SDK's own
`examples/` (matching their exact call patterns line for line wherever
possible) but **has not been compiled**: the SDK is Windows/MSVC-only
(`__declspec(dllexport)`, `__cdecl`, links against `D2RCore.dll`), so there
is no way to build or run it from this environment. **The first thing to do
with this checkout is a normal CMake + MSVC build, followed by a live-game
smoke test of all three buttons.** Several design decisions below are
first-guess placeholders specifically because they couldn't be verified
without that live test -- they're called out so you know what to check
first.

## Features

- **Auto Sort** button: runs the ported auto-sort algorithm (same
  category/quota layout as the save-editor) against the whole shared stash,
  configurable via `remote-stash-sorting.toml`.
- **Add Blank Page** button: moves the nearest empty tab after the plugin's
  tracked "selected tab" pointer to sit immediately after it (a pure
  tab-content rotation via item moves, not real tab creation -- the SDK has
  no such API).
- **Remove Current Page** button: the mirror operation -- pushes the
  selected tab (which must be empty) to the end of the stash.
- Two small arrow buttons track which tab the plugin considers "selected"
  (see "Known gaps" -- the SDK cannot tell the plugin which tab D2R has open).
- Results show in the plugin's own toast panel, per your explicit choice of
  "the plugin's own panel/toast" over D2RLoader's console or the D2R chat
  box.

## Architecture

```
src/
  sort_engine.hpp / .cpp        Phase 1: pure algorithm, no D2RLPlugin
                                 dependency. DefaultLayout(), RunAutoSort(),
                                 MoveBlankTabAfter(), PushBlankTabToEnd().
  item_classification.hpp / .cpp
                                 GearSlot/NonGearKind/isGear/isTwoHanded/
                                 width/height/base-levelreq lookup by packed
                                 item code, backed by a GENERATED table.
  item_classification_data.hpp  GENERATED (see tools/gen_classification.py)
                                 from the save-editor's base/*.txt files.
                                 Committed as a normal source file -- building
                                 this plugin does NOT require a checkout of
                                 D2R-Reimagined-Save-Editor.
  ui_layouts.hpp                Child-layout button-strip JSON (compile-time
                                 constant) and the toast panel JSON (built at
                                 runtime -- see "No dynamic widget text"
                                 below for why).
  plugin.cpp                    Phase 2: D2RLoaderGetPluginInfo /
                                 D2RLoaderLoadPlugin / D2RLoaderUnloadPlugin,
                                 service wiring, thread hops, the UI message
                                 listener that receives button clicks.
tools/
  gen_classification.py         Regenerates item_classification_data.hpp
                                 from a D2R-Reimagined-Save-Editor checkout's
                                 base/*.txt files. Only needed if you want to
                                 refresh the table (e.g. after a new D2R
                                 season changes item data) -- not part of the
                                 normal build.
remote-stash-sorting.toml       Default config, embedded into the DLL via
                                 d2rlplugin_embed_config() and copied out by
                                 D2RLoader on first load.
```

### Why no native hooks

`RuffnecKk-D2RLoader-Suite`'s `remote-stash` plugin does the same job (a
button on the inventory panel that manipulates shared-stash items) but
relies heavily on `InstallInlineHook` / raw native pointers to read and
write D2R's internal stash state. This plugin deliberately avoids that:
everything here goes through `InventoryService`, `ItemService`,
`PanelService`, `ResourceService`, `WidgetService`, `SharedEventService`,
and `ThreadService` -- all of which the SDK documents as stable, versioned,
ABI-checked surfaces. The tradeoff is that a few things RuffnecKk's suite
can read directly from memory (which tab is on screen, how many tabs exist)
have no supported equivalent yet and had to be worked around (see "Known
gaps").

### Threading

- Item mutation (`ItemService::executeExistingItemTransaction`) must run on
  the authoritative game thread (`ThreadService::runOnGameThread`).
- Widget/panel calls (`WidgetService::*`, `PanelService::openPanel`) must
  run on the UI thread (`ThreadService::runOnUiThread`).
- A button click arrives on the UI thread (`SharedEventService`'s
  `UiMessageListener`). Auto Sort / Add Blank Page / Remove Current Page hop
  UI thread -> game thread (compute the plan, apply the item moves) -> UI
  thread again (show the toast). The two tab-selector arrows need no item
  mutation, so they run directly from the UI-thread listener callback.

### UI message protocol

The button strip's `onClickMessage` fields send `"remote-stash-sorting:<Command>"`
(the D2R convention seen in the SDK docs and examples, e.g.
`"PanelManager:TogglePanel:<panel>"`). `plugin.cpp` registers one
`SharedEventService::UiMessageListener` that matches `target ==
"remote-stash-sorting"` and dispatches on `command`: `AutoSort`,
`AddBlankPage`, `RemoveCurrentPage`, `SelectPrevTab`, `SelectNextTab`. The
toast's Close button instead sends D2RCore's own built-in
`"PanelManager:ClosePanel:remote-stash-sorting/Toast"` message, so it needs
no plugin-side handler at all.

## Config (`remote-stash-sorting.toml`)

D2RLoader creates `<scope>/d2rloader/config/remote-stash-sorting.toml` from
the embedded default the first time the plugin loads, and never overwrites
a config you've already edited. See the file's own comments for the full
reference; the two settings you're most likely to need to change:

- `shared_stash_tab_count` -- **must match your actual shared stash tab
  count.** The SDK has no query for this (see "Known gaps"); Auto Sort fails
  cleanly with a toast + log message if it's set too low, so it's safe to
  round up if you're unsure.
- `[[layout]]` entries -- one per category, in fill order, mirroring
  `SS_AUTO_LAYOUT` from the save-editor. `mode_id` must be one of:
  `set`, `uniqueGear`, `rareGear`, `uniqueCharm`, `magicCharm`,
  `uniqueJewel`, `rareMagicJewel`, `uniqueRingAmu`, `rareRingAmu`,
  `magicRingAmu`. (The save-editor also has an `unidentified` mode; it isn't
  ported here -- see "Known gaps".)

## Known gaps (please review before relying on this)

These are the deliberate v1 scope decisions made while building this out.
None of them were explicitly signed off by you beforehand -- flagging them
here for review, same as the algorithm-level scope note below.

1. **`RunAutoSort` does not borrow/sweep/pad.** Unlike the save-editor's
   `_ssRunAutoSort`, this port does not borrow extra empty tabs when a
   category overflows its quota, does not sweep foreign items out of a
   target tab, and does not hand spare tabs to empty categories afterwards.
   A category that doesn't fit in exactly its configured `min_tabs` fails
   the whole run with a clear error instead of silently reflowing the
   layout. Covers the common case (stash roughly following the canonical
   layout already, or first-time sort with headroom); the
   borrow/sweep/pad behaviors are follow-up work. (`sort_engine.cpp`,
   `RunAutoSort` doc comment.)

2. **`sortLevel` is an approximation.** It only carries the base item's own
   `levelreq` (from `weapons.txt`/`armor.txt`/`misc.txt`, baked into
   `item_classification_data.hpp`). The save-editor's `_ssItemSortLevel`
   also raises it via `UniqueItems.txt`/`SetItems.txt`'s `lvl req` and the
   item's rolled affix `levelreq` -- both need row layouts
   `DataTableService` doesn't publish a typed struct for (`data_tables.h`
   hands back raw, undocumented row bytes). `itemLevel` (live, from
   `ItemInfo`) is the primary tie-break ahead of `sortLevel` in every sort
   key, so the practical impact is small (same-`itemLevel` ties only).

3. **No SDK query for the shared-stash tab count.** Worked around with the
   `shared_stash_tab_count` config value (see "Config" above) -- there is no
   way to discover or validate this automatically.

4. **No SDK query for "which tab is currently displayed."** Worked around
   with a plugin-tracked "selected tab" pointer (starts at 0, moved with the
   `< Tab` / `Tab >` buttons, shown in the toast) instead of the tab D2R
   itself has open. This is the biggest UX compromise in this plugin --
   ideally Add Blank Page / Remove Current Page would just know what's on
   screen. If a future SDK release exposes this (a UI message on stash-tab
   switch, or an `InventoryService` query), switching to it should be
   straightforward -- the plugin already isolates "what tab is selected"
   behind `g_selectedTab`.

5. **No dynamic widget text.** `WidgetService` has `getInputText` (reads
   `InputTextBoxWidget` content) and `setWidgetVisible`/`setWidgetEnabled`,
   but no "set widget text" call. There is therefore no supported way to
   push freshly-computed text (an item count, a specific error detail) into
   an already-registered JSON widget at runtime. The toast works around
   this by baking every possible outcome message into its own
   `TextBoxWidget` up front (see `ui_layouts.hpp`) and showing a result by
   toggling visibility, not by setting text -- so it can only show one of a
   fixed set of canned messages, never an exact item/tab count. Exact
   counts go to the plugin's log file (`PluginContext::LogInfo`) instead.
   The "selected tab: N" readout gets the same enumerated-widget treatment
   since its range is small and bounded (0..23, plus an overflow variant).

6. **Button-strip position/appearance, iteration 3 -- reusing a known-good
   spot and sprites instead of guessing.** `registerChildLayout` attaches
   `ui_layouts.hpp`'s `kButtonsChildLayoutJson` to
   `StockPanel::PlayerInventory` -- the *only* stock panel this SDK exposes
   to `registerChildLayout` (`Panels::StockPanel` has no `SharedStash`
   value), so the buttons can only ever live somewhere inside the
   `PlayerInventory` panel, never inside the Stash panel's own layout --
   that panel simply isn't an attachable target with this SDK version.
   Iteration 1 shipped with no explicit button `width`/`height`, so every
   button fell back to the native (large, modal-dialog-sized)
   `Panel\Modals\ModalButton` sprite size and all five visually stacked
   into one oversized, overlapping bar. Iteration 2 gave every button an
   explicit compact size but left position at `x=4,y=4` (top-left),
   unchanged and still overlapping the equipped-items paperdoll -- picking a
   truly empty spot inside `PlayerInventory` from screenshots alone took
   multiple guess-and-check rounds. Iteration 3 stops guessing: it copies
   `button_mold.sprite` and `charminvbutton.sprite` from the working
   `d2rl-charm-inv` plugin (its own small button attached to the edge of
   `PlayerInventory`) into a companion resource folder,
   `d2rl-remote-stash-sorting.mpq/data/hd/global/ui/d2rloader/remote-stash-sorting/`,
   structured exactly like `d2rl-charm-inv.mpq` itself (PluginSDK README
   "Resources and Companion MPQs": "D2RCore looks beside each plugin DLL for
   `d2rl-<plugin-id>.mpq`" -- a folder during development, a packed MPQ for
   release). Matching that convention required renaming the built DLL from
   `RemoteStashSorting.dll` to `d2rl-remote-stash-sorting.dll` (see
   `CMakeLists.txt`'s `OUTPUT_NAME`); **if you already have the old-named DLL
   in `<game>/d2rloader/plugins/`, delete it** so the plugin doesn't load
   twice under two different filenames. D2RCore discovers the companion
   folder on its own, purely by that filename convention. Auto Sort now sits
   at `{ x: 3, y: 672, width: 54, height: 141 }` with the mold graphic
   behind it, the *exact* spot `d2rl-charm-inv` already proves is empty on
   this same `PlayerInventory` child-layout coordinate space -- known-good,
   not a guess. The other four buttons stack directly below the mold as
   plain `charminvbutton` icon buttons (48x48/44x44, explicit sizes to avoid
   repeating the iteration-1 overlap regardless of the sprite's real native
   frame size). This is still a placeholder look (all five share one icon,
   distinguishable only by tooltip) meant to get the button strip clickable
   and off the paperdoll; `LogButtonStripRects` (see `plugin.cpp`) still
   logs the engine-resolved rect of the strip and every button via
   `WidgetService::getWidgetRect` whenever one is clicked, to confirm this
   iteration's numbers against the live game.

   **Iteration 4 -- the layout JSON and its text moved out of C++ too.**
   Iteration 3 still built the button-strip JSON as a C++ string constant
   (`kButtonsChildLayoutJson` in `ui_layouts.hpp`) and registered it at
   runtime via `ResourceService::registerResource`, even though `d2rl-charm-inv`
   itself ships its equivalent layout as a plain file
   (`InventoryButtonhd.json`) inside its companion `.mpq`. Since the button
   strip has no runtime-computed content (fixed coordinates, fixed sprite
   references, fixed tooltips) -- unlike the toast panel, which genuinely
   needs to be built at runtime for its enumerated per-outcome/per-tab
   widgets -- there was no reason to keep it in code. It is now a real file:
   `d2rl-remote-stash-sorting.mpq/data/global/ui/layouts/remote-stash-sorting/StashToolshd.json`,
   matching `data/global/ui/layouts/<plugin-id>/<local-id>hd.json` (the
   exact convention the PluginSDK README documents and `d2rl-charm-inv.mpq`
   itself uses). `plugin.cpp` no longer calls `registerResource` for it at
   all -- `registerChildLayout(StockPanel::PlayerInventory, "StashTools")`
   is enough; D2RCore resolves the file from the companion `.mpq` on its
   own, the same way it already resolved the sprites. Every button's
   `tooltipString` also stopped being a hardcoded English literal and now
   references a string-table key, e.g. `"@remote-stash-sorting:AutoSortTooltip"`,
   defined in a new
   `d2rl-remote-stash-sorting.mpq/data/local/lng/strings/d2rloader/remote-stash-sorting/strings.json`
   (confirmed supported on `tooltipString`, not just `TextBoxWidget` `text`,
   by `d2rl-charm-inv`'s own `CloseButton`: `"tooltipString": "@d2r:strClose"`).
   The toast panel's 14 outcome messages (`ui_layouts.hpp`'s
   `OutcomeMessage`) moved the same way -- `BuildToastLayoutJson()` still
   builds the panel JSON at runtime (still registered via
   `registerResource`, since the *set* of visible widgets is still dynamic),
   but each message's `text` is now `"@remote-stash-sorting:MsgAutoSortOk"`
   etc. instead of an embedded English sentence, resolved against the same
   `strings.json`. Net effect: every piece of *content* this plugin
   currently shows the player -- coordinates, sprite filenames, tooltip
   text, and toast wording -- now lives in `d2rl-remote-stash-sorting.mpq/`
   as editable JSON, not baked into the DLL; only genuinely dynamic
   structure (which toast widgets are visible, which tab number is shown)
   still goes through C++. `strings.json` currently only has an `enUS` entry
   per key -- add the other language columns `d2rl-charm-inv`'s own
   `strings.json` uses (`zhTW`, `deDE`, `esES`, `frFR`, `itIT`, `koKR`,
   `plPL`, `esMX`, `jaJP`, `ptBR`, `ruRU`, `zhCN`) whenever this plugin is
   ready to be localized -- no code changes needed for that, just more
   columns in the JSON.

7. **No confirmed way yet to show the buttons only while Shared Stash is
   open.** Requested behavior: the button strip should only appear when the
   Shared Stash is actually open, not for plain `PlayerInventory`. Nothing
   in this SDK version documents the native message D2R fires when Shared
   Stash opens/closes/switches tabs, and `Panels::StockPanel` has no
   `SharedStash` value to query directly. Added a debug aid instead of
   guessing: `debug_log_ui_messages = true` in the config makes
   `OnUiMessage` log every `target`/`command`/`text` it sees (not just this
   plugin's own) to the plugin log, so the real message can be found by
   toggling Shared Stash in-game and reading the log, then filtered on and
   used to drive `setWidgetVisible` on `StashTools` -- not implemented yet.
   Until then the button strip is visible whenever `PlayerInventory` is
   (i.e. also with plain inventory, no stash open).

8. **`forEachInventoryItem` + `ItemContainer::SharedStash` is assumed to
   enumerate every tab, not just the open one.** This is the only way
   `ItemInfo::sharedStashPage` existing at all makes sense, and it's what
   the whole plugin depends on, but it hasn't been confirmed against a live
   game. If it turns out to only return the currently-open tab's items, Auto
   Sort would only ever see one tab's worth of items.

9. **Multiplayer TCP/IP client: writes are host-only in v1.** A remote
   client's `executeExistingItemTransaction` call returns
   `Items::Result::NotAuthoritative` (per `item.h`'s documented behavior);
   the plugin catches this and shows a clear "not supported yet as a
   client" toast rather than silently failing. The real fix -- relay the
   computed plan to the host over `NetworkService`, have the host resolve
   `ItemHandle`s (which are per-process-local) via a stable identifier like
   `runtimeId`/`itemSeed`, and apply the transaction there -- was designed
   during planning but **is not implemented**. This was explicitly
   requested as in-scope ("Có, cần hoạt động cả khi là client"), so treat
   this as the most significant piece of unfinished work here, not a
   dropped requirement.

10. **A plan over 4096 operations loses cross-chunk atomicity.**
   `Items::MaxExistingItemOperations` caps one transaction at 4096; a plan
   larger than that (a near-maximally full, near-maximally sized stash) gets
   split into several transactions, each atomic on its own but not atomic as
   a whole. `ApplyMoves` logs a warning when this happens. Realistically
   rare (would need on the order of thousands of items in play at once).

11. **The `unidentified` sort mode isn't ported.** The save-editor has one;
    `DefaultLayout()` here mirrors the other ten `SS_AUTO_LAYOUT` entries
    only.

## Testing

- `src/sort_engine.cpp` and `src/item_classification.cpp` build cleanly
  under `g++ -std=c++20 -Wall -Wextra` (no D2RLPlugin dependency) and were
  exercised end to end with a synthetic scenario (unique gear, rare gear,
  unique/magic charms, a unique ring, and a 3-piece set) confirming correct
  category tab-range offsetting, atomic set-piece grouping into one tab, and
  the `MoveBlankTabAfter`/`PushBlankTabToEnd` tab-shift math.
- `src/plugin.cpp` builds cleanly with MSVC (x64, C++20, `/EHsc`) via
  CMake + Ninja. It was originally hand-checked against the exact struct
  layouts, function signatures, and calling patterns in
  `PluginSDK/include/D2RLPlugin/*.h` and `PluginSDK/examples/*/*.cpp`
  (config-file, ui-panel, widget-localization, item-transaction,
  shared-events, network-ping, game-rules were all read in full and matched
  line-for-line where applicable) before a real compiler was available to
  confirm it; two build-only issues turned up on the first real build (an
  MSVC 16.11/VS2019 internal compiler error on the `BuiltinModes()`
  designated-initializer aggregates -- fixed by building with a newer MSVC
  toolset, e.g. VS2022/2026, via the Ninja generator instead of a Visual
  Studio solution generator; and a toml++ "exceptions mode" API mismatch in
  `LoadConfig()`, fixed to use `try`/`catch` around `toml::parse()`). A live-
  game pass through all five buttons per "Known gaps" #6 and #7 above is
  still the next required step.

## Building, the local `deploy/` folder, and Releases

**Toolchain:** MSVC x64 (VS2022 or newer recommended -- an old VS2019 16.11
`cl.exe` is known to ICE on `sort_engine.cpp`'s `BuiltinModes()`, see
"Testing" above), C++20, CMake 3.29+, and the **Ninja** generator rather
than a Visual Studio solution generator (sidesteps CMake/VS-version-name
mismatches, e.g. CMake not yet knowing a brand-new VS release's generator
name). In VS Code's CMake Tools: select an MSVC kit, set
`"cmake.generator": "Ninja"` in settings, pick the Release variant, build.

**Local deploy folder:** every build (any config, VS Code or the command
line) copies the freshly built DLL -- named `d2rl-remote-stash-sorting.dll`,
not the CMake target's default `RemoteStashSorting.dll` (see
`OUTPUT_NAME` in `CMakeLists.txt`; required so D2RCore's
"`d2rl-<plugin-id>.mpq` beside the DLL" lookup finds the companion folder
below) -- plus its companion `d2rl-remote-stash-sorting.mpq/` resource
folder (the sprites from "Known gaps" #6) into `deploy/d2rloader/plugins/`
(see the `RSS_DEPLOY_PLUGINS_DIR` custom commands in `CMakeLists.txt`). The
whole `deploy/d2rloader` folder can be dragged straight into `<game>/`.
`deploy/` is gitignored -- it is regenerated by every build, not committed.
Optionally set `-DD2RLOADER_GAME_DIR=<path to your D2R install>` (e.g. in a
local, gitignored `CMakeUserPresets.json`) to also have every build copy the
DLL and its companion folder directly into
`<D2RLOADER_GAME_DIR>/d2rloader/plugins/` for live testing, no manual copy
step needed. **If you built this plugin before the DLL rename, delete the
old `RemoteStashSorting.dll` from your `<game>/d2rloader/plugins/`** so it
doesn't load twice under two different filenames.

**GitHub Releases:** `.github/workflows/release.yml` builds Release/x64 and
publishes a GitHub Release automatically, but only when you push a version
tag -- ordinary commits do not trigger it:

```
git tag v0.1.0
git push origin v0.1.0
```

The workflow checks out this repo and
[`D2RLoader/PluginSDK`](https://github.com/D2RLoader/PluginSDK) as sibling
folders (matching the local `D2RLoader-Plugins-Dev/{Remote-Stash-Sorting,
PluginSDK}` layout `CMakeLists.txt` expects), builds with the same
Ninja + MSVC combination as above, then zips the resulting
`deploy/d2rloader` folder as-is and attaches it to the Release -- so the
Release asset is already in "extract into `<game>/`" shape, with no separate
packaging step to keep in sync.

## Regenerating `item_classification_data.hpp`

Only needed if D2R's item data changes (a new season, a mod). Requires a
checkout of `D2R-Reimagined-Save-Editor` (for its `base/*.txt` files) next
to this plugin:

```
python3 tools/gen_classification.py
```

It reads `../D2R-Reimagined-Save-Editor/base/{itemtypes,weapons,armor,misc,
setitems}.txt` and rewrites `src/item_classification_data.hpp` in place.
