# Quyen19492 Item Sorting

A D2RLoader plugin that ports the shared-stash **auto-sort**, **add blank
page**, and **remove current page** features from
[D2R-Reimagined-Save-Editor](../D2R-Reimagined-Save-Editor) into a live,
in-game plugin, and adds a **Sort** button for the personal stash, the
inventory and the Horadric Cube (see "Single-grid sorting" below), built entirely on the official [D2RLoader Plugin
SDK](../PluginSDK) services. **It does not use `native_hook` /
`InstallInlineHook` / `hooks.h` / `mutations.h` anywhere** -- every feature
here (reading items, moving items, adding UI, catching button clicks) goes
through an officially supported service call. See "Why no native hooks"
below for why that constraint shaped several design decisions.

> **Renamed.** This plugin used to be `remote-stash-sorting`
> (`d2rl-remote-stash-sorting.dll` / `.mpq`, `remote-stash-sorting.toml`). It is
> now plugin id **`quyen19492-item-sorting`**: `d2rl-quyen19492-item-sorting.dll`,
> companion `d2rl-quyen19492-item-sorting.mpq`, config
> `quyen19492-item-sorting.toml` with its settings under `[item-sorting]`,
> and strings under the `@quyen19492-item-sorting:` namespace. When upgrading an
> install, delete the old `d2rl-remote-stash-sorting.dll`, `.mpq` folder and
> `.toml` from `<game>/d2rloader/plugins/` (otherwise both load), carry your
> settings over into the new `.toml`, and re-run
> `tools/patch_mod_stash_layout.py` so the mod's stash layout points at the new
> id (it recognises and replaces the old strip).
>
> **Renamed again.** The plugin id was then `quyen194-item-sorting`
> (`d2rl-quyen194-item-sorting.dll` / `.mpq`, `quyen194-item-sorting.toml`);
> it is now `quyen19492-item-sorting`. Only user-visible names changed (plugin
> id, DLL / `.mpq` / `.toml` / log file names, layout / panel / sprite /
> string-table paths, UI message target); the C++ namespace and CMake targets
> are simply `ItemSorting` / `ItemSortEngine` / `ItemSortTests`. Upgrading an
> install works the same way: delete the old `quyen194` DLL, `.mpq` folder and
> `.toml`, move your settings into `quyen19492-item-sorting.toml`, and re-run
> `tools/patch_mod_stash_layout.py` (strip markers are now
> `Quyen19492ItemSorting*Buttons_v<N>`; the old `Quyen194ItemSorting*` strips
> are recognised and replaced).

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
  configurable via `quyen19492-item-sorting.toml`.
- **Add Blank Page** button: moves the nearest empty tab after the plugin's
  tracked "selected tab" pointer to sit immediately after it (a pure
  tab-content rotation via item moves, not real tab creation -- the SDK has
  no such API).
- **Remove Current Page** button: the mirror operation -- pushes the
  selected tab (which must be empty) to the end of the stash.
- Two small arrow buttons track which tab the plugin considers "selected"
  (see "Known gaps" -- the SDK cannot tell the plugin which tab D2R has open).
- **Sort** buttons for the personal stash, the inventory and the Horadric
  Cube: each arranges its one grid by category (zones, corner anchors, locked
  cells, keep-in-place categories), configured per container -- see
  "Single-grid sorting" below.
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
  item_misc_data.hpp            GENERATED (tools/gen_misc_classification.py)
                                 misc.txt kinds rune/gem/potion/tool/quest.
  grid_sort.hpp / .cpp          Single-grid sort (personal stash, inventory,
                                 cube): pure algorithm, PlanSort().
  grid_config_toml.hpp          TOML -> Grid::Config for those sections.
  ui_layouts.hpp                Button/layout notes and the toast's widget
                                 names (OutcomeMessage, tab readout) that
                                 plugin.cpp shares with the static
                                 Toasthd.json in the companion mpq -- see
                                 "No dynamic widget text" below.
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
  gen_misc_classification.py    Regenerates item_misc_data.hpp from a mod's
                                 excel folder.
  patch_mod_stash_layout.py     Inserts the stash / cube buttons into the
                                 mod's own layout files.
tests/
  grid_sort_test.cpp            Single-grid engine + config + classification
                                 checks (no SDK needed).
quyen19492-item-sorting.toml       Default config, embedded into the DLL via
                                 d2rlplugin_embed_config() and copied out by
                                 D2RLoader on first load.
```

### Why no native hooks

`RuffnecKk-D2RLoader-Suite`'s `remote-stash` plugin does the same job (a
button on the inventory panel that manipulates shared-stash items) but
relies heavily on `InstallInlineHook` / raw native pointers to read and
write D2R's internal stash state. This plugin deliberately avoids that:
everything here goes through `InventoryService`, `ItemService`,
`PanelService`, `WidgetService`, `SharedEventService`,
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

The button strip's `onClickMessage` fields send `"quyen19492-item-sorting:<Command>"`
(the D2R convention seen in the SDK docs and examples, e.g.
`"PanelManager:TogglePanel:<panel>"`). `plugin.cpp` registers one
`SharedEventService::UiMessageListener` that matches `target ==
"quyen19492-item-sorting"` and dispatches on `command`: `AutoSort`,
`AddBlankPage`, `RemoveCurrentPage`, `SelectPrevTab`, `SelectNextTab`. The
toast's Close button instead sends D2RCore's own built-in
`"PanelManager:ClosePanel:quyen19492-item-sorting/Toast"` message, so it needs
no plugin-side handler at all.

### Log tags

D2RLoader prefixes every line in the plugin's log with
`[Plugin:quyen19492-item-sorting]`. After that, each message starts with the
feature it is about: `[remote-stash]` (shared stash: Auto Sort, Add / Remove
Page, on-screen page tracking), `[personal-stash]`, `[inventory]`, `[cube]`,
`[config]` (reading `quyen19492-item-sorting.toml`) and `[plugin]` (loading,
service checks, the `debug_log_ui_messages` firehose). `LogTagFor()` in
`plugin.cpp` maps a container to its tag.

Shared-stash pages and grid cells are **1-based in every log line** (page 1
is the first page, matching the in-game "Page N / 200" label; cell (1,1) is
the top-left), even though the SDK and the engine count both from 0. Use
`ItemSorting::PageText` / `CellText` (`sort_engine.hpp`) for any new log
text. The one exception is the single-grid scan's "raw inventoryPage id",
which is the game's internal page id, not a page a player counts.

A rejected shared-stash move logs the item's code, category, size, where the
scan found it and where the plan sent it; after the run, one summary line
counts the rejections per source page and per destination page.

## Config (`quyen19492-item-sorting.toml`)

D2RLoader creates `<scope>/d2rloader/config/quyen19492-item-sorting.toml` from
the embedded default the first time the plugin loads, and never overwrites
a config you've already edited. See the file's own comments for the full
reference. Structure:

| Section | Holds | Read |
| --- | --- | --- |
| `[item-sorting]` | plugin-wide: `enabled` (master switch -- false registers nothing), `notification_*`, `debug_log_ui_messages`, `mod_data_path` | once, at game start |
| `[item-sorting.shared_stash]` + `[[...shared_stash.layout]]` | shared stash: its own `enabled`, `add_page_enabled`, `remove_page_enabled`, `grid_width`/`grid_height`, `shared_stash_tab_count`, `start_tab`, `blank_tabs_at_start`, `compact_empty_tabs_first`, `add_empty_page_when`, the category list | on every Auto Sort / Add Page / Remove Page press; `enabled` and the two page toggles also whenever the stash opens (to show/hide the buttons) |
| `[item-sorting.personal_stash]` / `.inventory` / `.cube` | single-grid sorting (see "Single-grid sorting") | on every Sort press |

Shared-stash keys left directly under `[item-sorting]` by an older
config file (or the old `[item-sorting.grid]` width/height and
`[[item-sorting.layout]]`) are ignored with a log warning naming the
new location. The section names themselves were `[quyen194-item-sorting...]`
(the plugin id at the time) before; a file still using those is ignored as a
whole (built-in defaults, with a log warning) -- only the file name keeps the
`quyen19492-` prefix, since
D2RLoader names it after the plugin id. `[shared_stash] enabled = false` hides all three shared-stash
buttons (same `setWidgetVisible` mechanism as the page toggles) and ignores
their actions.

The settings you're most likely to need to change:

- `shared_stash_tab_count` -- **must match your actual shared stash tab
  count.** The SDK has no query for this (see "Known gaps" #3); Auto Sort
  fails cleanly with a toast + log message if it's set too low. A live run
  hit `Result::Unsupported` for a handful of items landing in the two
  highest tabs the padding pass used, which first looked like this value
  being set too high -- but the player confirmed their stash genuinely has
  that many tabs, so the actual cause is still open (see "Known gaps" #3).
  Until it's found, don't assume a too-high value is safe just because this
  run's cause turned out to be something else.
- `add_page_enabled` / `remove_page_enabled` (default `true`) -- set to
  `false` to hide the shared stash's Add Page / Remove Page button and ignore
  that action. The buttons are widgets in the mod's patched
  `BankExpansionLayout`, so the plugin hides them at runtime
  (`WidgetService::setWidgetVisible`, applied after every
  `PanelManager:OpenPanel:BankExpansionLayout` and
  `BankPanelMessage:SelectTab`); the hidden button's slot in the row stays
  empty. Both keys are re-read every time the stash opens -- no restart.
- `[[layout]]` entries -- one per category, in fill order, mirroring
  `SS_AUTO_LAYOUT` from the save-editor. `mode_id` must be one of:
  `set`, `uniqueGear`, `rareGear`, `magicGear`, `superiorGear`, `normalGear`,
  `uniqueCharm`, `magicCharm`, `uniqueJewel`, `rareMagicJewel`,
  `uniqueRingAmu`, `rareRingAmu`, `magicRingAmu`, `unclassified`. (The
  save-editor also has an `unidentified` mode; it isn't ported here -- see
  "Known gaps" #11.)
- `sort_keys` / `gear_groups` (per `[[layout]]` entry) -- this category's
  own sort order and, for gear categories, its own slot/weapon-type bucket
  order. See "Known gaps" #14 below and the config file's own comments for
  the full accepted-key reference; every entry has a sensible built-in
  default, so neither field needs to be set unless you want to change the
  ordering.
- `mod_data_path` -- leave empty (`""`, the default) to classify items
  using the table embedded in the DLL at build time. Set it to a mod's own
  folder (the one containing `data/global/excel/itemtypes.txt` etc. -- e.g.
  `D:\\Games\\DiabloII-Resurrected\\DiabloII-Resurrected\\mods\\Reimagined\\Reimagined.mpq`)
  to have the plugin parse that mod's own txt files fresh, every Auto Sort
  run, instead. Falls back to the embedded table with a log warning if the
  path or its files can't be read. See "Known gaps" #3 for why this exists.
- Everything outside the plugin-wide `[item-sorting]` block
  re-reads from disk on every button press (see the table above) -- edit and
  save the file, then press the button again, no game restart needed. See
  "Known gaps" #14.

## Known gaps (please review before relying on this)

These are the deliberate v1 scope decisions made while building this out.
None of them were explicitly signed off by you beforehand -- flagging them
here for review, same as the algorithm-level scope note below.

1. **`RunAutoSort` borrows, and the padding pass is now partially
   ported.** A user reported hitting the borrow gap directly: a `min_tabs`
   of 5 for `uniqueGear` read (correctly) as a *minimum reservation*, not
   a hard cap, and the original port treated it as the latter -- failing
   outright instead of growing into more tabs the way the save-editor's
   `_ssRunAutoSort` / `_ssPrepareInPlaceSort` does. Ported: a category that
   doesn't fit in its `min_tabs` now borrows one more tab at a time (from
   right after its reserved run, up to the shared stash's last tab) and
   retries, exactly mirroring `_ssPrepareInPlaceSort`'s incremental-borrow
   loop -- only failing once the *entire* stash has no tab left to give it.
   The category's actual span (reserved + borrowed) advances the cursor,
   so later categories simply start further along, same as the
   save-editor's reflow. Unlike the save-editor, this never needs to
   physically relocate a tab's raw page content to make borrowed tabs
   contiguous -- this plugin moves items individually via live
   `ItemService` transactions, so a destination tab is just a number, not
   a page-splicing constraint the save-editor's raw-byte editing has to
   work around.
   On top of that, a second requested pass (`RunAutoSortWithPadding` in
   `sort_engine.cpp`) now ports the save-editor's `autosort.json`
   blank-tab/padding config as a second layer on top of the plan above:
   `compact_empty_tabs_first` (relabel already-empty tabs to the end of
   the active range before sorting, mirroring
   `_ssCompactEmptyTabsToEnd`), `add_empty_page_when` -- global and
   per-category, mirroring `_ssWantsEmptyPage` -- (add one "breathing"
   blank tab after a category once its last used tab is at least this %
   full; `"never"`/`"always"`/a number, `"always"` and `0` are
   equivalent), and `blank_tabs_at_start` (pull that many empty tabs to
   the front of the active range after everything else is placed,
   mirroring `_ssAddBlankTabsAtStart`). See "Config" above and
   `quyen19492-item-sorting.toml`'s comments for the exact settings.
   Deliberately **not** ported: the save-editor's pass-1 quota-shortfall
   redistribution (its `padding: roundRobin|inOrder|off` setting, which
   hands an empty category's unused `min_tabs` quota to others *after* the
   fact) -- this plugin's `RunAutoSort` always reserves a category's full
   `min_tabs` immediately even when it turns out empty, so there's no
   shortfall left for a pass 1 to redistribute; an empty category already
   ends up with `min_tabs` blank tabs left in place by the main sort
   itself. Also not ported: the `profiles`/`activeProfile` convenience-
   preset layer, `continueOnFailure`, `skipConfirm`, and sweeping foreign
   items out of a target tab (the save-editor's `strayItems` handling).

   **A first live run caught a real bug in this combined plan**: the same
   item routinely gets moved more than once across phases (compacted into
   a closed-up tab, then placed somewhere else entirely by the category
   sort; or resettled again by a later padding/blank-tab rotation), and
   `ExistingItemTransaction` rejects the *entire* chunk it's in with
   `Result::InvalidArgument` if any item handle appears in it more than
   once -- so the raw concatenation of all four phases' moves could fail
   to commit anything at all (`committed 0/33361`, on a stash where the
   individually-correct plan was 3537 items), even though
   `DetectCollisions`'s self-check came back clean (it already dedupes to
   each handle's final move for its own spatial check, which is exactly
   what wasn't happening to what actually got sent). Fixed by collapsing
   `RunAutoSortWithPadding`'s combined `plan.moves` to one move per item
   handle -- keeping only its last occurrence -- right before returning,
   so every handle the game sees appears exactly once regardless of how
   many phases touched it and regardless of chunk boundaries. This also
   cuts the real number of item-move operations substantially, since most
   items no longer pay for an earlier phase's now-superseded move.
   (`sort_engine.cpp`, end of `RunAutoSortWithPadding`; `plugin.cpp`,
   `TryParseEmptyPageRule`/`LoadConfig`.)

   **A second live run (after the commit fix above) surfaced the actual
   cause of the sparse-page complaint that started this whole thread**:
   `RunAutoSort` seeds each category's occupancy grid from every existing
   item still sitting in its target tabs, so it never places its own item
   on top of one that hasn't moved yet -- correct for a genuinely
   unclassified/leftover item, which really never moves. But it was doing
   the same for every OTHER item too, including ones that match a
   *different* category in the very same `layout` and are themselves about
   to get their own fresh move later in this same `RunAutoSort` call.
   Combined with `compact_empty_tabs_first` -- which closes every gap in
   the active range, so essentially the WHOLE STASH's pre-sort content
   (every category mixed together) now sits contiguously right where the
   *first* category in `layout` (`uniqueGear`) is about to place its own
   items -- that first category ended up planning around nearly the
   entire rest of the stash's clutter as if none of it would ever move,
   forcing it to borrow tab after mostly-empty tab until it finally
   escaped into genuinely untouched territory. This is exactly what the
   screenshots showed: a page with one item and a wall of empty cells,
   while a much later page in the same category still had items. A
   synthetic stash built to match the user's real log (356 uniqueGear /
   158 uniqueRingAmu / 95 rareRingAmu / 325 magicRingAmu / 19 uniqueCharm
   / 299 magicCharm / 57 uniqueJewel / 99 rareMagicJewel / 358 set / 1771
   rareGear / 53 unmatched, all categories interleaved and pre-packed
   contiguously the way `compact_empty_tabs_first` would leave them)
   reproduced it directly: `uniqueGear` needed 92 tabs (5 reserved + 87
   borrowed) for its 356 items, and the whole plan used 166 tabs total.
   Fixed by precomputing, once per `RunAutoSort` call, the set of item
   handles that match ANY category in `layout` at all -- and excluding
   every one of them from occupancy seeding, in every category's `tryFit`,
   not just the category that will eventually claim them. Only a truly
   unclassified item (matches nothing in `layout`) still blocks placement,
   which is the only case where that's actually true. On a synthetic
   stash built to match the user's real log this dropped `uniqueGear` to
   11 tabs and the whole plan to 85 tabs total -- a ~2x reduction overall,
   concentrated almost entirely in whichever category runs first. A live
   run confirmed the density win directly: `uniqueGear` went from 78 tabs
   for 356 items (the very first live run, before any of these fixes) down
   to 9 tabs for the same 356 items, and the whole plan settled at 72
   tabs used total.

   **A third live run (after the density fix above) surfaced a real
   collision** -- `RunAutoSort`'s own self-check caught 2 different items
   both planned onto the exact same cell, and the game correctly refused
   the whole transaction (`Result::Conflict`) rather than silently
   corrupting anything. The cause was one specific gap in "is this item
   actually going to get a move" that `willBeMoved` (see above) didn't
   know about: the `"set"` mode's stream `match()` only checked
   `quality == Set`, but the atomic-set placement loop *also* silently
   skips any item whose `setGroupName` is empty -- which happens whenever
   `SetGroupForRow` can't map `qualityRecordId` to a known set (see its
   own doc comment in `item_classification.hpp`: such an item is meant to
   be "treated the same as a completely unknown item", i.e. left alone,
   never moved). `willBeMoved` trusts `stream.match()` as its single
   source of truth for "will this handle get a move" -- so a Set-quality
   item with an unresolved group name was wrongly marked as "about to
   vacate" everywhere, while the actual placement loop never gave it a
   move at all. It still got relabeled by the *compact* phase though
   (`CompactEmptyTabsToEnd` doesn't care about classification, only about
   closing tab-number gaps), landing it at a real, occupied cell inside
   `set`'s own tab range -- which the (buggy) occupancy seeding then told
   a different real set group was free to use too. Two items, one cell,
   caught by the self-check exactly as designed. Fixed by making the
   `"set"` mode's `match()` require a non-empty `setGroupName` itself
   (`IsSetQuality(it) && !it.setGroupName.empty()`), so it's the single
   accurate answer to "will this item actually be placed" that both
   `willBeMoved` and the placement loop agree on (the placement loop's own
   `setGroupName.empty()` check is now redundant, kept only as a
   defensive belt-and-suspenders). Reproduced in isolation two ways: (1)
   `RunAutoSort` alone confirms a real set group no longer lands on top of
   an unresolved-group-name item's cell within `set`'s own tab range, and
   (2) the full `RunAutoSortWithPadding` pipeline (compact relabeling the
   stuck item into the exact cell a second group would otherwise claim)
   reproduces the *exact* production collision with the old `match()` and
   confirms zero collisions with the fix. `DetectCollisions` only catches
   this class of bug when the "stuck" item also happens to get a
   compact-phase move (so it appears in the moves list at all) -- an item
   that never moves in ANY phase and still gets planned over would not be
   caught by the self-check today; worth keeping in mind if a similar
   "match() is broader than what actually gets placed" gap ever creeps
   into another mode. Live-verified: the collision is gone and the
   density win holds (`uniqueGear` 78->9 tabs, 356 items; whole plan 72
   tabs). (`sort_engine.cpp`, the `"set"` mode's `match()` in
   `BuiltinModes()`.)

2. **`sortLevel` was an approximation -- now fixed.** It used to carry only
   the base item's own `levelreq` (from `weapons.txt`/`armor.txt`/
   `misc.txt`). That sounded like a minor same-`itemLevel`-ties-only gap, but
   a live report ("reqLevel sorting looks jumbled on the ring/charm pages")
   traced back to something much bigger: `Rings.txt`/`Charms.txt`/
   `Jewels.txt`/`Amulets.txt` each have exactly **one** row, with
   `levelreq == 0`, so `baseLevelReq` never varied across an entire ring,
   amulet, charm, or jewel category regardless of quality or roll -- the
   `"reqLevel"` sort key was a complete no-op there, and every visible order
   was actually being decided by whatever key came next (`"-ilvl"`, then
   name), which has no relationship to the item's own displayed "Required
   Level". Gear categories were affected too, just less visibly (base armor/
   weapon types do vary in `levelreq`, so `reqLevel` wasn't a *total* no-op
   there, just an approximation).

   `Classification::TrueReqLevel()` (`item_classification.hpp`/`.cpp`) now
   ports the save-editor's `_ssItemSortLevel`/`_ssAffixLevelReq` in full,
   using fields the SDK's `item.h` documents plainly (no undocumented
   `DataTableService` row-layout guessing needed after all):
   `ItemInfo::qualityRecordId` is a valid row index into `UniqueItems.txt`
   for `quality == Unique`, not just `SetItems.txt` for `quality == Set` --
   both are now read (`lvl req` column) and applied. `ItemInfo::prefixIds`/
   `suffixIds` (up to 3 each, populated for magic/rare/crafted items) are the
   *combined* MagicSuffix+MagicPrefix+AutoMagic ID space item.h describes
   ("with 10 suffix rows, MagicPrefix table-local ID 1 has combined ID 11");
   `TrueReqLevel()` decodes each nonzero ID against `magicprefix.txt`/
   `magicsuffix.txt`'s own `levelreq` column and takes the highest, ignoring
   an ID that lands in the AutoMagic range (same as the save-editor, which
   never consults `AutoMagic.txt` for this). The four new lookup tables this
   needed (`kSetLevelReqByRow`, `kUniqueLevelReqByRow`,
   `kMagicPrefixLevelReq`, `kMagicSuffixLevelReq`) are baked into
   `item_classification_data.hpp` by `tools/gen_classification.py` exactly
   like `kTable`/`kSetGroupByRow` already were, and are also available
   through `mod_data_path` (see "Config" above) for a mod whose data has
   drifted from the bundled reference copy.

   Building this surfaced a second, more general finding: the
   `kSetGroupByRow` off-by-one row this README previously described as "a
   genuinely unused/reserved row in Blizzard's own data" is actually D2R's
   own literal `"Expansion"` row -- a classic/expansion section delimiter
   present in `setitems.txt`, `uniqueitems.txt`, `itemtypes.txt`,
   `weapons.txt`, `armor.txt`, and `misc.txt` alike (confirmed against the
   reference save-editor's own parser, which explicitly skips any row whose
   first column is exactly `"Expansion"`). It only ever mattered for
   `setitems.txt` before because that was the only table indexed by row
   *position*; adding `uniqueitems.txt` row-position indexing for the fix
   above would have silently reproduced the exact same off-by-one bug for
   Unique items had this not been caught first. Both `gen_classification.py`
   and `mod_item_data.cpp`'s table reader now skip a literal `"Expansion"`
   row explicitly (the general, correct rule) rather than inferring it from
   blank columns (the narrower rule the original fix happened to get right
   by luck, for `setitems.txt` specifically).

   Validated offline: parsing the same reference txt files these tables are
   generated from and diffing all four new arrays against the embedded ones
   found zero mismatches, and `TrueReqLevel()` was checked against known
   real values (Raven Frost's Unique row -> 45, Natalya's Totem's Set row ->
   59, and both a suffix- and a prefix-affix combined-ID decode).

3. **No SDK query for the shared-stash tab count.** Worked around with the
   `shared_stash_tab_count` config value (see "Config" above) -- there is no
   way to discover or validate this automatically.

   A live run on a 3853-move plan still hit 73 `Result::Unsupported`
   rejections, landing in the two tabs the padding pass / Add Blank Page
   (`PadAutoLayoutBreathingRoom` / `AddBlankTabsAtStart`, built on
   `RotateEmptyTabTo`, see "Known gaps" #13) shifted the "unclassified"
   catch-all category into -- never the category sort itself. The tab-count
   theory above was wrong (player confirmed all 200 tabs are real), and a
   follow-up run with the rejection log now printing each item's code and
   category (added to `ApplyMoves` in `plugin.cpp`) **confirmed the
   catch-all-item hypothesis outright**: every one of the 73 rejections was
   a `quality == 0` item -- gems, runes, quest/essence items, and (a second,
   initially-confusing cluster) this mod's own crafting materials (Orbs of
   Conversion/Corruption/Renewal/Socketing/Shadows, Uber Ancient Summon
   Materials, Worldstone Shards, Rune/Jewel Pliers, Keys of
   Destruction/Hate, Gem Cluster, Experience Potion). That second cluster
   logs as `gear(slot=0)` rather than `unclassified(...)` only because
   `item_classification_data.hpp`'s generator -- faithfully matching the
   save-editor's own `_ssIsGearItem` -- calls anything that isn't
   specifically a charm/jewel/ring/amulet/gem/socket-filler "gear", even
   when (as here) it also fails to match any of the 23 real gear slots and
   ends up `GearSlot::None`. Both clusters are the exact same underlying
   thing: a `quality == 0` item with no dedicated sort category, swept into
   `Mode::catchAll` ("unclassified") along with everything else.

   **What's now ruled out:** a capacity/tab-allocation bug in
   `sort_engine.cpp` itself. Replaying the identical algorithm offline
   against a real export of the post-run stash (same classification table,
   same config, same occupancy) places all ~238 `quality == 0` items
   cleanly into just 2 tabs (1 reserved + 1 borrowed) with room to spare --
   it never reproduces a shortfall. The planning/bin-packing logic is not
   the problem.

   **Leading hypothesis, then narrowed further.** The rejections were
   originally assumed to be the *category plan's own* real destinations
   landing in a borrowed/padded-in tab the player had never visited. A
   later run's log told a more precise story: the plan's own real
   footprint (every category's actual placement) fit entirely within tabs
   `[0, tabsUsed)` (`outcome.tabsUsed`, printed in the "auto sort placed ...
   across N tab(s)" line) -- but the specific rejected moves targeted tab
   numbers a couple past that, tabs the plan itself never placed a single
   real item into. That points at `SequenceMovesForLiveApply`'s dependency-
   cycle breaker instead (`sort_engine.cpp`; see `SequencedMoves::
   isScratchMove`'s own doc comment): to unblock a cycle (A wants B's spot,
   B wants A's spot -- common in a full reshuffle), it temporarily parks one
   member in a scratch cell, found by `FindScratchCell` scanning tabs
   upward from 0 for the first free one. In a nearly-full stash, tabs the
   plan's own categories actually use can have too little slack left over
   for that search, so it spills into tabs beyond the plan's real footprint
   -- bare `shared_stash_tab_count` padding the plan never touches. If a
   tab out that far turns out to be one the player has never opened (this
   plugin's earlier hypothesis) -- or, more simply, one that doesn't
   correspond to a real tab at all because `shared_stash_tab_count` is set
   higher than the account's actual shared stash size (see this setting's
   own note above: there's no SDK query to validate it) -- `Unsupported` is
   exactly what a Move into it would get, and the scratch cell search had
   no way to know either was the case; it just found the first cell that
   LOOKED unoccupied.

   **Fix:** `SequenceMovesForLiveApply`/`FindScratchCell` now take an
   optional `preferredMaxTab` (plugin.cpp passes `outcome.tabsUsed`) and
   search `[0, preferredMaxTab)` -- tabs the plan itself just proved safe by
   placing real items there -- before ever falling back to the padding
   tail beyond it, which it still will if that narrower range genuinely has
   no free cell (never makes an otherwise-possible scratch pick fail, just
   reorders where it looks first). `ApplyMoves`'s rejection log also now
   says outright when a rejected move was one of these synthetic scratch
   hops rather than an item's real destination, so this no longer has to be
   re-derived by eye from which tabs the rest of the plan touches. Validated
   with a dedicated offline test: a stash packed so tightly that its own
   footprint has zero free cells forces the exact fallback-to-padding path
   this fix is meant to avoid taking unnecessarily, while a stash with even
   one free cell left in its own footprint is confirmed to use that instead
   of ever looking at the padding tail.

   This narrows, but does not by itself prove, the remaining open question:
   whether a genuinely never-opened (but real) tab actually is unsupported
   for a live Move, or whether these particular rejections were simply
   `shared_stash_tab_count` set higher than the real tab count all along. If
   you're seeing this warning, checking that setting against your actual
   shared stash size (see "Config" above) is worth doing before assuming
   the deeper, unconfirmed live-API quirk -- and if rejections do still
   occur, the log will now say plainly whether they're this scratch-cell
   case or something else.

   Either way, thanks to "Known gaps" #15's per-item isolation this already
   degrades as gracefully as this class of problem can: a handful of
   individually-logged, isolated rejections, with the rest of the plan
   committing normally.

   **Separate bug found while chasing this one -- confirmed and fixed.** A
   Set-quality item could sit forever in its original spot, untouched by
   Auto Sort, with *no* rejection ever logged for it at all -- because it
   was never even added to a category's plan in the first place.
   `plugin.cpp` fills `ItemFacts::setGroupName` (`CollectSharedStashItem`)
   via `Classification::SetGroupForRow(item->qualityRecordId)`, and
   `sort_engine.cpp`'s "set" category's `match()` requires that name to be
   non-empty (an unresolved set item is deliberately "treated the same as
   a completely unknown item" and left alone, on purpose, rather than
   moved into an unknown group). A live report of exactly this -- a Grim
   Helm ("Natalya's Odium") sitting untouched across a full Auto Sort run
   that clearly ran the "set" category and placed hundreds of *other* set
   items successfully -- came with the smoking gun once
   `CollectSharedStashItem` was updated to log the raw `qualityRecordId`
   on an empty lookup: **`qualityRecordId=62`**.

   `base/setitems.txt` (the same file `gen_classification.py` reads to
   build `kSetGroupByRow`) has exactly one fully-blank interior row --
   both its `item` and `set` columns empty, a genuinely unused/reserved
   row in Blizzard's own data, not a parsing artifact -- at raw row
   position 62, immediately before "Natalya's Odium"'s 4 rows (63-66).
   The old generator emitted a `""` placeholder for that row and still
   counted it as a table slot, so every row from 63 onward sat exactly
   one position "ahead of" where the live game -- which does not hand out
   a `qualityRecordId` for a row with no actual item -- expects it.
   `qualityRecordId=62` for a real "Natalya's Odium" item is exactly what
   that off-by-one predicts.

   Confirmed there is only one such fully-blank row in the whole 456-row
   file, so the fix is narrow and low-risk: `gen_classification.py` now
   skips emitting a slot at all for a fully-blank row instead of giving it
   an empty placeholder slot, and `item_classification_data.hpp` has been
   regenerated (`kSetGroupByRow` is now 455 entries; `kTable`, the item
   classification table, is untouched). Every set from "Natalya's Odium"
   onward -- including "Volf's Undead Legion" at what's now index
   223-226 -- shifts down by exactly one row to line up with the live
   game's numbering. `CollectSharedStashItem`'s diagnostic log (item code
   + raw `qualityRecordId` whenever the lookup still comes back empty)
   stays in place in case a different, still-unknown misalignment shows up
   somewhere else in the table.

   The blank-row bug above is really a special case of a broader risk:
   `item_classification_data.hpp` is a snapshot, generated once at build
   time (`tools/gen_classification.py`) from a *bundled reference copy* of
   `itemtypes/weapons/armor/misc/setitems/uniqueitems/magicprefix/
   magicsuffix.txt` (the last three feed `TrueReqLevel()` -- see gap #2
   above) -- not from whatever mod is actually active in the running game.
   If a mod's data ever diverges from that bundled copy (a future Reimagined
   patch renumbers `setitems.txt` rows again, or you point this plugin at a
   different mod entirely), the embedded tables go stale and
   Classify()/SetGroupForRow()/TrueReqLevel() start lying again, silently,
   exactly like this bug did. There's no SDK-level fix for this today (the
   SDK's `DataTableService` doesn't publish typed row structs for these
   tables either -- see gap #2 above), so as a practical mitigation there is
   now a `mod_data_path` setting (see "Config" above): point it at a mod's
   own folder and the plugin parses that mod's *own* txt files fresh, every
   Auto Sort run, instead of trusting the embedded snapshot. Leave it empty
   to keep using the embedded table (the default, and still the right
   choice if you don't know your mod's data folder path). This was
   validated offline by parsing the same reference txt files these tables
   were generated from and diffing every one of the 724 classification
   entries, all 455 `kSetGroupByRow`/`kSetLevelReqByRow` rows, all 1483
   `kUniqueLevelReqByRow` rows, and all 1117/823
   `kMagicPrefixLevelReq`/`kMagicSuffixLevelReq` rows against the embedded
   tables: zero mismatches.

4. **No SDK query for "which tab is currently displayed."** Worked around
   with a plugin-tracked "selected tab" pointer (starts at 0, moved with the
   `< Tab` / `Tab >` buttons, shown in the toast) instead of the tab D2R
   itself has open. This is the biggest UX compromise in this plugin --
   ideally Add Blank Page / Remove Current Page would just know what's on
   screen. If a future SDK release exposes this (a UI message on stash-tab
   switch, or an `InventoryService` query), switching to it should be
   straightforward -- the plugin already isolates "what tab is selected"
   behind `g_selectedTab`.

   **Update (live-verified):** the page is now tracked from events instead of
   the `<`/`>` buttons alone. `WidgetService` still can't read it
   (`getInputText` on `BankExpansionLayout`'s `CurrentSharedStashText`
   TextBoxWidget returns `Unsupported`; `findWidget`/`getWidgetRect` return
   identical results whichever stash tab is selected), but D2R broadcasts
   `BankPanelMessage:SharedStashRight` / `SharedStashLeft` for the native page
   arrows, `BankPanelMessage:SelectTab` (no index) for the stash tabs, and
   `PanelManager:OpenPanel:BankExpansionLayout` when the stash opens. The
   plugin sets the page exactly from `ItemTooltipEvent` (hovering a
   shared-stash item -> `ItemInfo::sharedStashPage`), follows the arrows +/-1
   from there, and forgets it when the stash reopens or an arrow is pressed on
   the first/last page. Add Blank Page / Remove Current Page refuse to run
   (toast `MsgPageUnknown`) while the page is unknown.

5. **No dynamic widget text.** `WidgetService` has `getInputText` (reads
   `InputTextBoxWidget` content) and `setWidgetVisible`/`setWidgetEnabled`,
   but no "set widget text" call. There is therefore no supported way to
   push freshly-computed text (an item count, a specific error detail) into
   an already-registered JSON widget at runtime. The toast works around
   this by baking every possible outcome message into its own
   `TextBoxWidget` up front (the static
   `data/global/ui/layouts/quyen19492-item-sorting/Toasthd.json` in the
   companion mpq, whose widget names come from `ui_layouts.hpp`; the test
   suite checks every name is present) and showing a result by
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
   `d2rl-quyen19492-item-sorting.mpq/data/hd/global/ui/d2rloader/quyen19492-item-sorting/`,
   structured exactly like `d2rl-charm-inv.mpq` itself (PluginSDK README
   "Resources and Companion MPQs": "D2RCore looks beside each plugin DLL for
   `d2rl-<plugin-id>.mpq`" -- a folder during development, a packed MPQ for
   release). Matching that convention required renaming the built DLL from
   `Quyen194ItemSorting.dll` to `d2rl-quyen19492-item-sorting.dll` (see
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
   `d2rl-quyen19492-item-sorting.mpq/data/global/ui/layouts/quyen19492-item-sorting/StashToolshd.json`,
   matching `data/global/ui/layouts/<plugin-id>/<local-id>hd.json` (the
   exact convention the PluginSDK README documents and `d2rl-charm-inv.mpq`
   itself uses). `plugin.cpp` no longer calls `registerResource` for it at
   all -- `registerChildLayout(StockPanel::PlayerInventory, "StashTools")`
   is enough; D2RCore resolves the file from the companion `.mpq` on its
   own, the same way it already resolved the sprites. Every button's
   `tooltipString` also stopped being a hardcoded English literal and now
   references a string-table key, e.g. `"@quyen19492-item-sorting:AutoSortTooltip"`,
   defined in a new
   `d2rl-quyen19492-item-sorting.mpq/data/local/lng/strings/d2rloader/quyen19492-item-sorting/strings.json`
   (confirmed supported on `tooltipString`, not just `TextBoxWidget` `text`,
   by `d2rl-charm-inv`'s own `CloseButton`: `"tooltipString": "@d2r:strClose"`).
   The toast panel's 14 outcome messages (`ui_layouts.hpp`'s
   `OutcomeMessage`) moved the same way -- `BuildToastLayoutJson()` still
   builds the panel JSON at runtime (still registered via
   `registerResource`, since the *set* of visible widgets is still dynamic),
   but each message's `text` is now `"@quyen19492-item-sorting:MsgAutoSortOk"`
   etc. instead of an embedded English sentence, resolved against the same
   `strings.json`. Net effect: every piece of *content* this plugin
   currently shows the player -- coordinates, sprite filenames, tooltip
   text, and toast wording -- now lives in `d2rl-quyen19492-item-sorting.mpq/`
   as editable JSON, not baked into the DLL; only genuinely dynamic
   structure (which toast widgets are visible, which tab number is shown)
   still goes through C++. Later still, the toast layout itself moved out of
   C++ too: `BuildToastLayoutJson()` and its `registerResource` call are
   gone, and the panel is a plain `Toasthd.json` file in the companion mpq
   (found by `registerPanel("Toast")` like the `Action*` panels), so its
   coordinates can be tweaked without rebuilding the DLL. `strings.json` currently only has an `enUS` entry
   per key -- add the other language columns `d2rl-charm-inv`'s own
   `strings.json` uses (`zhTW`, `deDE`, `esES`, `frFR`, `itIT`, `koKR`,
   `plPL`, `esMX`, `jaJP`, `ptBR`, `ruRU`, `zhCN`) whenever this plugin is
   ready to be localized -- no code changes needed for that, just more
   columns in the JSON.

   **Iteration 5 -- buttons were visible but unclickable; root rect was
   likely clipping hit-testing.** Live test: the strip rendered correctly
   (5 icons visible in the expected spot, confirmed by screenshot), but
   clicking any of the 5 buttons produced no effect at all -- not even
   `LogButtonStripRects`' unconditional debug log line, which only fires
   once `OnUiMessage` sees an event with `target == "quyen19492-item-sorting"`.
   15k+ lines of `debug_log_ui_messages` firehose from that test session
   contained zero such lines, meaning the click never reached our
   `ButtonWidget`s at all -- not a downstream logic bug. Leading suspect:
   `StashTools`' own root `Widget` rect was `{ x: 0, y: 0, width: 1, height:
   1 }` -- a 1x1 anchor point, sized that way on purpose since children can
   render outside their parent's declared rect. Rendering apparently isn't
   clipped to a parent's bounds, but *input hit-testing plausibly is* (a
   common scene-graph pattern: descend into a child subtree only if the
   click first falls within the parent's own rect) -- which would explain
   visible-but-dead buttons exactly. Fix: widened the root rect to `{ x: 0,
   y: 0, width: 110, height: 980 }`, covering every child button's actual
   bounding box, **without changing its `x`/`y`** (child rects are local
   offsets from the parent's `x`/`y` per `widgets.h`, so moving the parent's
   position would have shifted every button on screen instead of just
   fixing hit-testing).

   **Iteration 6 -- root-rect fix confirmed deployed and retested; still no
   click.** Verified this time (not just committed): rebuilt clean, redeployed,
   and the rect-fix commit's `StashToolshd.json` (`width: 110, height: 980`)
   was confirmed present in `deploy/` and the timestamps on the built DLL/mpq
   are *after* the fix, ruling out "stale build" as an explanation. A fresh
   `debug_log_ui_messages` firehose taken right after clicking still shows the
   `LeftMouseButtonDown`/`LeftMouseButtonUp` pair but no
   `target="quyen19492-item-sorting"` line anywhere near it -- so widening the
   root rect did not fix it; that hypothesis is falsified. New data point from
   the user: the clicked button *does* visibly go to its pressed frame
   (`hoveredFrame`/press art), meaning the `ButtonWidget` itself is receiving
   the mouse-down under the cursor -- the click is not simply missing the
   widget's screen bounds. Yet `onClickMessage` never fires. Two diagnostics
   were added to narrow this further without needing more guessing:
   - `LogButtonStripRects` no longer waits for our own `target` to match
     (that match has *never* been observed, so gating the diagnostic behind it
     meant it had literally never run). It now fires once, automatically, the
     first time any `UiMessage` arrives after the `StashTools` child layout
     exists -- giving the engine-resolved rects unconditionally, without
     needing a working click first.
   - A self-test (`RunSelfTestDispatch`) is queued via `runOnUiThread` right
     after the listener registers: it calls
     `WidgetService::dispatchUiAction` with `target="quyen19492-item-sorting"`,
     `command="SelfTest"` -- the exact same broadcast shape a real button
     click is documented to use (PluginSDK README, "Existing Widgets":
     "send a normal target/command/text action"). If the log shows
     `self-test UiMessage received`, the listener registration and
     target/command filtering are proven correct end-to-end, which would mean
     the bug is entirely on the engine's side -- something is preventing the
     click from ever reaching `dispatchUiAction` for this widget's
     `onClickMessage`, not a bug in how this plugin listens for it.

   Leading new suspect: `registerChildLayout` can only attach to
   `StockPanel::PlayerInventory` (see above -- `SharedStash` isn't an
   attachable target). In normal play, opening the Shared Stash shows it
   *alongside* the player's inventory on the same screen. If the Stash
   panel is a separate, higher-priority overlay with its own full-panel
   click-catching (the working reference plugin `d2rl-charm-inv` does
   exactly this in its own panel -- a `ClickCatcherWidget` spanning a large
   rect specifically to intercept clicks before whatever is underneath),
   that would explain everything observed: rendering is unaffected (it's a
   separate concern from click routing), the button's own local
   hover/press visual still updates from raw cursor position, but the
   actual click-resolution hit-test never reaches into the
   `PlayerInventory` child layout because the Stash panel's overlay wins it
   first. This does not require a rebuild to test: open **only** the player
   inventory (press the inventory key without interacting with a stash),
   so the Shared Stash panel never opens, and click one of the five
   buttons.

   **Iteration 7 -- root cause found: onClickMessage cannot use a
   plugin-invented target; found via `d2rloader.log`, not
   `debug_log_ui_messages`.** The click-catcher theory above was never
   tested -- a look at D2RLoader's own `d2rloader.log` (separate from this
   plugin's per-plugin log) turned up the real cause first. Right around the
   point the `PlayerInventory` panel is built, it logs three native
   assertions from `D2UI\src\TypeDesc.cpp`, all about the literal string
   `"quyen19492-item-sorting"`: `BC_ASSERT: hashValue != m_hashValues.end()`
   ("Given key not found", `valueName = "quyen19492-item-sorting"`),
   `BC_ASSERT: channel.IsValid()`, and `BC_ASSERT: false` ("Missing message
   value list registration for %s", `buf = "quyen19492-item-sorting"`).
   Assertion dialogs are disabled, so D2R just logs and silently continues
   -- no crash, no visible error, nothing in this plugin's own log, nothing
   in the `debug_log_ui_messages` firehose either, because the failure
   happens *before* a UiMessage is ever broadcast. Reading between the
   three messages: a `ButtonWidget.onClickMessage`'s `target` is validated
   against a native, closed registry of "message channels" (`TypeDesc`'s
   "message value list") when the JSON layout is parsed -- built-in names
   like `PanelManager`, `InputMessage`, `MonsterHealth` are pre-registered;
   an arbitrary plugin id like `"quyen19492-item-sorting"` is not, and never
   becomes a registered channel just by calling
   `SharedEventService::registerUiMessageListener` (that only makes the
   plugin a listener for messages that get broadcast -- it does not
   register the plugin's own id as a valid broadcast target). So every one
   of the five buttons' `onClickMessage` values
   (`"quyen19492-item-sorting:AutoSort"` etc.) failed this native validation
   silently, every single time, and the click was dropped before it could
   ever produce a `UiMessageEvent` -- fully explaining both symptoms at
   once: the button still shows its native pressed frame (purely a local,
   cursor-driven visual, unrelated to `onClickMessage`), and no
   `target="quyen19492-item-sorting"` line, and no click-triggered
   `LogButtonStripRects` output, ever appeared in any log, at any rect,
   with or without the root-rect fix.

   The fix: stop inventing a custom `onClickMessage` target entirely.
   `PanelManager` *is* a pre-registered channel (it's how `d2rl-charm-inv`'s
   own button already works: `"PanelManager:TogglePanel:charm-inv/CharmInvPanel"`),
   and `PanelService` exposes `openPanel`/`closePanel`/`togglePanel` as
   direct C++ calls too. So each button now targets
   `"PanelManager:TogglePanel:quyen19492-item-sorting/Action<Name>"` against
   one dedicated, invisible (`1x1`, no children) trigger panel per action --
   five new panels (`ActionAutoSort`, `ActionAddBlankPage`,
   `ActionRemoveCurrentPage`, `ActionPrevTab`, `ActionNextTab`), registered
   in `D2RLoaderLoadPlugin` alongside the existing `Toast` panel, with
   layout files
   `d2rl-quyen19492-item-sorting.mpq/data/global/ui/layouts/quyen19492-item-sorting/Action<Name>hd.json`.
   `OnUiMessage` now matches `target == "PanelManager"` and
   `command == "TogglePanel"`, looks up `text` (the toggled panel's own
   logical name) against the five trigger panels to find which button
   fired, calls `closePanel` on it right away so the *next* click toggles
   it open again instead of closed, and dispatches the matching action --
   same `DoAutoSort`/`DoAddBlankPage`/etc. handlers as before, just reached
   through a native, pre-registered channel instead of an invented one.
   Unverified against the live game as of this writing.

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
   rare (would need on the order of thousands of items in play at once). As
   of item #15 below, this chunking never crosses a dependency-layer
   boundary -- a single *layer* over 4096 operations is what actually
   triggers the warning now, not the plan as a whole.

11. **The `unidentified` sort mode isn't ported.** The save-editor has one;
    `DefaultLayout()` here mirrors the other `SS_AUTO_LAYOUT` entries only.
    Since #14 below added a dynamic `unclassified` catch-all, an
    unidentified item is no longer silently left wherever it started (it
    lands in `unclassified` like any other item with no dedicated
    category) -- it just doesn't get the save-editor's own dedicated
    `unidentified` column layout.

12. **Item placement within a tab is best-fit, not monotonic -- a
    deliberate deviation from the save-editor, by request.** The
    save-editor's `_ssPlaceMonotonic` never looks behind its scan cursor
    when placing an item, by its own design comment: "Places one item
    without ever looking behind the cursor, so the on-screen reading order
    always matches the sort order... Costs a few empty cells; that
    trade-off is deliberate." A user found this produced visibly sparse
    tabs (empty gaps on one page while a later page in the same category
    still had items) and asked for denser packing instead, explicitly
    accepting the loss of that strict order-preservation guarantee.
    `PlaceBestFit` (`sort_engine.cpp`, replacing the removed
    `PlaceMonotonic`) re-scans every free cell in a tab from the start for
    each item, so tabs pack as densely as the grid allows; item order
    within a tab can now be slightly out of the original scan order as a
    result. `min_tabs`/borrowing and the padding pass above are
    unaffected -- this only changes where an item lands *within* its
    already-chosen tab.

13. **Rotating an empty tab into position picks the nearest one, not the
    farthest.** `_ssAddBlankTabsAtStart` in the save-editor scans backward
    from the end of the save file for an empty tab to swap forward,
    because a byte-level page swap there is O(1) regardless of distance.
    This plugin has no raw pages to swap -- "moving" an empty tab means
    relabeling every real item's `sharedStashPage` between the source and
    destination via individual `ItemService` moves -- so `RotateEmptyTabTo`
    (`sort_engine.cpp`, shared by the padding pass and
    `blank_tabs_at_start`) deliberately picks the *nearest* empty tab
    instead, minimizing the number of item-move operations. This mirrors
    the convention already used by `MoveBlankTabAfter`/`PushBlankTabToEnd`
    elsewhere in this plugin. It also means `RotateEmptyTabTo` cannot tell
    a genuinely empty real tab apart from a tab number beyond the actual
    live stash, in principle -- see "Known gaps" #3 for a live case that
    looked like exactly this and turned out not to be.

14. **Quality-tier gear categories, a dynamic "unclassified" catch-all,
    config hot-reload, and per-category `sort_keys`/`gear_groups` -- added
    by request.** Before this, a magic/superior/normal-quality gear item
    (anything that wasn't unique or rare) matched no category at all: it
    wasn't moved, wasn't counted in `itemsSorted`, and wasn't mentioned in
    the plan log -- it just silently stayed wherever it started. Two
    changes close that gap. First, three new modes --
    `magicGear`/`superiorGear`/`normalGear` -- cover those three quality
    tiers the same way `uniqueGear`/`rareGear` already covered theirs.
    Second, a new `unclassified` mode is a dynamic catch-all: unlike every
    other mode, its match predicate isn't a fixed quality/type check --
    `RunAutoSort` computes it fresh on every call as "every item not
    claimed by any *other* mode actually present in this layout" (see
    `Mode::catchAll`/`streamMatches` in `sort_engine.cpp`). This also means
    removing any category from `[[layout]]` entirely (e.g. commenting out
    `rareGear`) no longer strands its items -- they simply fall through to
    `unclassified` instead, with no special-casing needed per category.
    `unclassified` is meant to go last in the layout so it never steals
    tabs from a real category ahead of it.

    On top of the new categories, every category's sort order is now
    config-driven instead of a hardcoded per-mode comparator: a
    `CategorySpec::sortKeys` list of named keys (`reqLevel`, `ilvl`, `name`,
    `quality` everywhere; `gearGroup`/`handTier` for gear categories;
    `setName`/`itemName` for `set`; `size` for the two charm categories),
    each optionally prefixed `-` for descending, resolved into a
    `CompoundKey` and compared field by field (`ResolveKeyField`/
    `BuildSortKey`/`CompoundKeyLess` in `sort_engine.cpp`). A gear category
    can also override `gear_groups`, its own priority order for the 23
    slot/weapon-type buckets the `gearGroup` key uses
    (`BuildGearGroupPriority`). Leaving either field unset falls back to
    that mode's own `defaultSortKeys`/the default bucket order, chosen to
    reproduce this plugin's original hardcoded ordering exactly -- so an
    existing config with no `sort_keys`/`gear_groups` at all sorts
    identically to before this change.

    Finally, `DoAutoSort` now re-runs `LoadConfig` at the top of every Auto
    Sort call and refreshes `g_config`'s tab count, layout (including every
    category's `sort_keys`/`gear_groups`), `start_tab`,
    `blank_tabs_at_start`, `compact_empty_tabs_first`, and
    `add_empty_page_when` from the fresh read -- only `enabled`,
    `debug_log_ui_messages`, and `[grid]` stay fixed at their plugin-load-
    time values, since those three need a game restart to take effect. So
    editing the config and clicking Auto Sort again is now enough to see a
    layout/sort-order change, no restart required.

    Validated with `test_new_features.cpp` (25 scenario assertions:
    catch-all coverage when a category is removed from the layout,
    catch-all not stealing from a real category ahead of it, `gear_groups`
    reordering actually changing placement order, a `sort_keys` override
    actually changing ordering vs. the built-in default, and the atomic
    `set` mode's `setName`/`reqLevel`/`-ilvl`/`itemName` group-and-item
    ordering in both directions) and by re-running a real 3450-item shared
    stash (extracted from an actual `.d2i` save) through the new engine: it
    now places all 3450 items (previously 5 magic/superior/normal-quality
    gear items had no category and were silently excluded from the plan),
    with zero collisions in `DetectCollisions`'s self-check.

15. **A live run with the new categories above (item #14) surfaced a much
    bigger, pre-existing problem in how the plan is actually sent to the
    game -- `ApplyMoves` needed two separate fixes.**

    **First**, a real Auto Sort on a 3616-item stash committed
    `0/3616 (transaction result 2)`. `ExistingItemTransactionResult`'s own
    doc comment explains why: `committedOperationCount is zero on
    failure` -- one operation anywhere in a multi-thousand-item transaction
    that the game rejects (e.g. an item it doesn't consider movable from
    wherever it actually is) sinks every other, otherwise-fine operation
    riding in the same call. A single 4096-operation-chunked loop had no way
    to tell "one poison operation" apart from "everything is broken."
    Fixed by having `ApplyMoves` bisect a failing chunk on any non-`Success`
    result, retrying each half separately and recursing down to individual
    operations when necessary, so a plan still commits everything it safely
    can and only the genuinely-rejected operation(s) are skipped (logged by
    handle/destination/result code). Live-verified: the same plan went from
    `0/3616` to `1849/3616` committed, with each of the ~1767 rejections now
    individually logged instead of the whole sort silently doing nothing.

    **Second**, that 1849/3616 partial result turned out to be a much
    bigger problem than "a few bad items": the ~1767 rejections were split
    between `Result::Unsupported` and `Result::Conflict`, concentrated in
    contiguous handle ranges each landing in one tab -- a systemic pattern,
    not isolated bad data. The real cause: `RunAutoSort` computes every
    item's *final* destination by filling a layout it builds fresh in
    memory, with no regard for what is *currently*, physically sitting in
    that cell live in-game. Since a full Auto Sort repositions most or all
    of the stash, most destinations are, at the moment `ApplyMoves` tries to
    move into them, still occupied by some *other* item this same plan is
    also about to move elsewhere -- e.g. item A's destination is currently
    item B's position, but B hasn't moved yet because its own move comes
    later in the plan (or in a different, not-yet-applied chunk). The live
    Move API correctly rejects that with `Conflict`; no amount of retrying
    the *same* order fixes it, since nothing frees up between retries.
    `DetectCollisions`'s self-check didn't catch this because it only
    replays moves into a *fresh* grid checking the final layout for
    internal consistency -- it was never checking planned destinations
    against *live*, pre-sort occupancy, which is a different question.

    Fixed with a new function, `SequenceMovesForLiveApply`
    (`sort_engine.hpp`/`.cpp`), inserted between planning and `ApplyMoves`
    in `DoAutoSort`/`DoAddBlankPage`/`DoRemoveCurrentPage`. It collapses the
    plan to one net move per handle (same reasoning `DetectCollisions`
    already uses), builds a dependency graph from *live* current occupancy
    against each move's destination footprint (`move(A) depends on
    move(B)` whenever B currently sits somewhere inside A's destination),
    and topologically sorts it into **layers**: every move in one layer is
    independent of every other move in that same layer, so a layer is safe
    to send to the live game as a single batch regardless of whether
    operations within one transaction call can see each other's effects
    (the SDK's own doc comment is ambiguous on this -- it documents a
    `failureIndex`, suggesting in-order attempts, but also a full rollback
    to zero on any failure) -- but a layer must fully finish before the next
    one starts, since later layers depend on earlier ones having actually
    happened live. A dependency **cycle** (A wants B's current spot, B wants
    A's -- or a longer loop of the same shape) has no valid one-at-a-time
    order at all without a spare cell, so one member of each cycle found is
    temporarily relocated to a scratch cell (proven free: not currently
    occupied by any live item, and not any move's own final destination)
    to break it, then moved on to its real destination once the rest of the
    cycle resolves -- the standard "sort in place with one spare slot"
    trick. `ApplyMoves` itself got simpler as a result: it no longer needs
    to guess at ordering, just bisect/chunk freely *within* each layer
    (`SequencedMoves::layerSizes`) and never batch *across* a layer
    boundary.

    Validated three ways, since `plugin.cpp` can only ever be built with
    real MSVC (see "Testing" below) so the live SDK call itself can't be
    exercised here: (1) `test_sequence_moves.cpp` (27 assertions) checks
    `SequenceMovesForLiveApply` directly against an independent replay
    judge for a simple dependency chain, a 2-cycle, a longer cycle mixed
    with an unrelated chain, an already-in-place no-op move, multi-cell
    (non-1x1) footprints straddling more than one current occupant, and a
    300-item dense reshuffle containing 150 disjoint 2-cycles -- confirming
    every produced order is conflict-free when replayed live, every layer
    is internally independent, and every handle's true final destination
    always matches the original plan regardless of any scratch hop needed
    to get there; (2) `apply_moves_bisect_test.cpp` mocks the transaction
    API's all-or-nothing behavior to confirm `ApplyMoves`' bisection still
    isolates a poison operation correctly; (3) `end_to_end_apply_test.cpp`
    links the real `sort_engine.cpp` (no mocking) against a mock live-game
    transaction API modeling the SDK's documented semantics literally
    (sequential in-order attempt, full rollback on any failure) and runs a
    deliberately worst-case ~3600-item full-stash reshuffle -- a near-total
    reversal permutation packing over 1300 simultaneous, independent swap
    cycles, far more adversarial than any real category-based sort would
    produce. Result: 99.4% of the plan committed live in only 187 calls
    (vs. the original bug's 0%, or the bisection-only fix's ~51%), with
    every one of the remaining handful of failures (large items caught in
    a scratch-cell pile-up specific to this synthetic worst case) still
    individually attempted and cleanly logged rather than silently dropped.
    (`sort_engine.hpp`/`.cpp`: `SequenceMovesForLiveApply`, `OwnerGrid`,
    `FindScratchCell`; `plugin.cpp`: `ApplyMoves`, and its three call sites.)

    **Confirmed live**, on the same 3616-item stash this was diagnosed
    against: committed went from `1849/3616` to `3543/3616`, and every
    `Result::Conflict` rejection is gone -- the fix above works as designed.
    The remaining 73 rejections were a *different*, narrower problem (100%
    `Result::Unsupported`) that first looked tab-related -- concentrated
    entirely in tabs 78 and 79 -- but is not a `shared_stash_tab_count`
    misconfiguration (the player confirmed their real tab count genuinely
    covers both tabs). Per "Known gaps" #3, the leading hypothesis now is
    that it is about the ITEM being moved, not the destination: gems,
    runes, quest items, and potions are the only things the
    "unclassified" catch-all category ever sends to those tabs, and are
    the only items rejected in either live run so far -- pending direct
    confirmation from an enhanced rejection log added this session.
    **That hypothesis is now ruled out:** the next run (log lines with item
    codes) rejected plain gear (`ka3`, `xa1`, `ua2`, ...) too, still all
    `Unsupported`, still all bound for 0-based tabs 78/79 (in-game pages 79
    and 80). Yet screenshots of those two pages show one item from each
    destination group did land there (Heavy Bracers at page 79 cell (1,1),
    Ral at page 80 cell (7,2)), so the destination pages accept moves; the
    rejections look tied to the items or where they came from (their
    handles are one contiguous run, 345-417). The rejection log now also
    prints each item's scanned source page/cell and size, plus a per-source-
    page summary, to settle this on the next run. It's
    still a good demonstration of this item's fix working exactly as
    intended even for a failure it wasn't built for: the 73 bad operations
    were isolated and individually logged, and every other operation in
    the same plan still committed normally.

## Testing

- `tests/grid_sort_test.cpp` (single-grid sorting): parses the shipped
  `quyen19492-item-sorting.toml` with the plugin's own TOML reader and checks
  every single-grid section parses and validates cleanly and matches
  `Grid::DefaultConfig`; config-error detection; misc-item classification
  (and, given a mod `.mpq` folder argument, that the runtime `mod_data_path`
  parser agrees with the generated table); hand-checked placement scenarios
  (anchors, zones, bands, overflow/compact, locked cells, items outside the
  grid); and 1,200 randomized grids checking no overlaps, nothing moved into
  locked cells, fixed items untouched, and that re-sorting a sorted grid is a
  no-op. Build with `-DQIS_BUILD_TESTS=ON` or the g++ line in the file's
  header, and run it from the repo root.
- `src/sort_engine.cpp` and `src/item_classification.cpp` build cleanly
  under `g++ -std=c++20 -Wall -Wextra` (no D2RLPlugin dependency) and were
  exercised end to end with a synthetic scenario (unique gear, rare gear,
  unique/magic charms, a unique ring, and a 3-piece set) confirming correct
  category tab-range offsetting, atomic set-piece grouping into one tab, and
  the `MoveBlankTabAfter`/`PushBlankTabToEnd` tab-shift math. `test_new_
  features.cpp` (25 assertions, item #14) and `test_sequence_moves.cpp` (27
  assertions, item #15) link against the real `sort_engine.cpp` the same
  way and build/run identically. `apply_moves_bisect_test.cpp` and
  `end_to_end_apply_test.cpp` (item #15) validate `ApplyMoves`' bisection
  and the full plan-to-live-transaction path against a mock transaction API
  standing in for the parts that genuinely need `plugin.cpp`/the live SDK
  (see below).
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

## Single-grid sorting: personal stash, inventory, Horadric Cube

Each of these three containers gets its own **Sort** button (all three reuse
the Auto Sort icon; the tooltip says which container it sorts):

| Container | Button | Added by |
| --- | --- | --- |
| Personal stash | stash dialog, Personal tab, bottom-left (x=110, y=1508) -- exactly **under** the shared stash's Auto Sort button. Personal and Shared share one container and D2R has no Personal-only one, so the strip is inserted before `SharedStashTabContainer`, which draws on top: on the Shared tab Auto Sort covers it. If one click ever reaches both, `DoSortGrid` drops the personal sort (clicks within 500 ms) | `tools/patch_mod_stash_layout.py` (`basicstash_container`) |
| Inventory | inventory panel, bottom-right, mirroring the RuffnecKk Remote Stash chest on the left of the gold field (x=956, y=1640; the first position, x=110, sat under that chest) | the plugin itself: child layout `InventoryTools` on `StockPanel::PlayerInventory` (`InventoryToolshd.json` in the companion MPQ), keyboard/mouse only |
| Horadric Cube | cube panel under the grid (x=120, y=1046), and on the stash dialog's cube tab (x=284, y=1540) | `tools/patch_mod_stash_layout.py` (`HoradricCubeLayout`, `advancedstash_horadriccube`) |

Each button fires `PanelManager:TogglePanel:quyen19492-item-sorting/ActionSort<PersonalStash|Inventory|Cube>`
(same trigger-panel pattern as the shared-stash buttons).

**Config:** `[item-sorting.personal_stash]`, `.inventory` and `.cube`
in `quyen19492-item-sorting.toml`, one `[[...layout]]` list each. The file's own
comments are the full reference (every key, every allowed value). Everything
is re-read on every Sort press, `enabled` included. A section missing from an
older config file falls back to built-in defaults identical to the shipped
file (`Grid::DefaultConfig`, kept in sync by the test harness). A key with an
invalid value makes that container refuse to sort (toast + the exact problem in
the log) -- it never silently falls back to some other value.

**Algorithm** (`src/grid_sort.*`, pure, no SDK dependency):

1. Items outside `grid_width` x `grid_height` are ignored (logged).
2. Items overlapping `locked_cells`, matching `keep_in_place`, or claimed by
   no layout entry (when there is no `unclassified` entry) stay where they are
   and act as obstacles.
3. Every other item goes to the first layout entry whose mode matches it. In
   these containers the gear modes only take real gear (`GearSlot != None`):
   the gear table marks potions, quest items and Reimagined's orbs/stacks/dyes
   as `isGear` with no slot, which is harmless across 200 shared tabs but
   would scatter them through a single grid.
4. Entries with a `zone` are placed first (inside the zone, spilling into the
   rest of the grid outside every zone), then the others. Each entry sorts its
   items by its `sort_keys` and places them first-fit, scanning from its
   `anchor` corner in `fill_direction` order; with `category_break = "band"`,
   the next zone-less entry with the same anchor starts in a fresh
   column/row.
5. If something doesn't fit: `on_overflow = "compact"` retries once without
   bands, otherwise (or if that fails too) nothing is moved.
6. `Grid::SequenceForLiveApply` turns the plan into single-item steps that
   each land on cells no **other** item occupies at that moment (an item may
   overlap its own old cells), parking blockers on free cells where items
   block each other; the whole sequence is simulated first, and if the grid
   is too full to find one, nothing moves. Each step is then its own
   one-operation transaction, stopping at the first rejection; afterwards the
   container is re-read and item count / handles / overlaps are checked and
   logged (`post-sort check`).

   **Why not one transaction:** the first version sent the whole plan as a
   single `executeExistingItemTransaction`, trusting the SDK README's "moved
   items are removed from a temporary occupancy model first, so a single
   transaction can swap or chain their locations". Live, every move reported
   committed, but items moved onto cells that another item of the same plan
   still occupied vanished from the grid once that item moved away (the cube
   and both tomes in the inventory, most of the personal stash). The shared
   stash path was never exposed to this -- its layered `ApplyMoves` only
   batches moves onto already-free cells -- except for its "cycle could not be
   broken" fallback, which used to send such a move anyway; `ApplyMoves` now
   refuses the whole plan instead (`SequencedMoves::unsafe`).

**New item categories** `rune`, `gem`, `potion`, `tool`, `quest` come from
misc.txt's `type` column through itemtypes.txt's Equiv chain
(`ItemFacts::miscKind/miscOrder/miscGrade`, `Classification::ClassifyMisc`),
backed by the generated `src/item_misc_data.hpp`
(`tools/gen_misc_classification.py`) or, with `mod_data_path` set, by the
mod's own misc.txt at runtime (`mod_item_data.cpp`, same rules). They are
valid in the shared-stash layout too. New sort keys: `area`, `height`,
`runeNumber`, `gemType`, `gemGrade`, `potionType`, `potionGrade`. Nothing in
the shared stash's existing layout or behavior changes.

**To verify on the first live run** (none of this could be tested outside the
game):

1. Button positions and clicks: the inventory button's coordinates assume the
   child layout uses PlayerInventory-panel coordinates (like the old
   `StashTools` strip); the stash/cube positions are from the mod's own layout
   files. Adjust `InventoryToolshd.json` / the `Strip` constants in
   `patch_mod_stash_layout.py` if anything overlaps.
2. The scan log line (`[personal-stash] scanned N item(s) [page P: N]`):
   Reimagined's Gems / Materials / Runes stash tabs hold items too. If they
   are reported as `PersonalStash` items on another inventory page, the sort
   **refuses** (toast + log) rather than risk pulling them into the grid --
   send that log line so the filter can be made exact.
3. Grid sizes: the shipped values are Reimagined's (16x13 / 10x8 / 12x8, read
   from the mod's layout files). An "outside the grid" log line means a value
   is too small.
4. One-transaction moves inside Inventory / PersonalStash / Cube (the SDK
   documents them; the shared stash is the only container live-tested so far).

## Buttons inside the stash dialog and cube panel (`tools/patch_mod_stash_layout.py`)

The plugin no longer attaches a button strip to the inventory panel. Its three
buttons -- **Auto Sort** (sort icon), **Add Page** (+) and **Remove Page** (-)
-- live in the shared-stash dialog itself. The SDK can only attach plugin UI to
`StockPanel::PlayerInventory`, and a plugin companion MPQ may not override
native layouts, but the active mod's own MPQ can, so
`tools/patch_mod_stash_layout.py` inserts a 3-button strip into the mod's
`data/global/ui/layouts/bankexpansionlayouthd.json`, inside
`BankExpansionLayout > basicstash_container > SharedStashTabContainer`, on the
dark strip left of the native "Page N / 200" navigation. That container is only
shown on the Shared tab, so the buttons hide on the other stash tabs by
themselves. Each button fires `PanelManager:TogglePanel:quyen19492-item-sorting/Action*`.

The button art is this plugin's own. The source artwork lives in
`assets/stash_buttons/{autosort,addpage,removepage}.png`;
`tools/gen_stash_button_sprites.py [--size 88] [--png]` (Pillow) crops it,
builds 88x88 native `SpA1` sprites with 4 frames (normal, hovered, pressed,
disabled -- the native page-arrow order; only "normal" is artwork, the rest
are derived) plus half-size `.lowend` copies, and writes them into the
companion MPQ at
`data/hd/global/ui/d2rloader/quyen19492-item-sorting/stash_btn_*.sprite`.
Keep `--size` in sync with `BUTTON_SIZE` in `tools/patch_mod_stash_layout.py`.

```text
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq            # patch / upgrade (no-op if current)
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq --check    # exit 0 current, 2 not patched / older strip
python tools/patch_mod_stash_layout.py <game>/mods/Reimagined/Reimagined.mpq --remove   # take it out again
```

Every write first copies the file to `.bak`, `.bak1`, `.bak2`, ... (first free
name; backups are never overwritten), and the result is re-parsed as JSON and
checked before it is written. The strip carries a version marker
(`Quyen19492ItemSortingStashButtons_v<N>`), so an older strip is replaced in place.
Re-run it after each mod update, since the mod's installer replaces the layout
files. `--controller` also patches the `controller/` copies (off by default:
the buttons need a mouse anyway).

Besides the shared-stash strip, the script now also inserts the single-grid
Sort buttons (each strip has its own version marker, so they are checked,
upgraded and removed independently):

| Strip | File | Container | Marker |
| --- | --- | --- | --- |
| Sort Personal Stash | `bankexpansionlayouthd.json` | `basicstash_container` (first child, under Auto Sort -- see above; v1 was the last child at x=398 and is upgraded in place) | `Quyen19492ItemSortingPersonalButtons_v3` |
| Sort Cube (stash cube tab) | `bankexpansionlayouthd.json` | `advancedstash_horadriccube` (last child; skipped if the mod has no such tab) | `Quyen19492ItemSortingCubeTabButtons_v2` |
| Sort Cube | `horadriccubelayouthd.json` | `HoradricCubeLayout` (last child) | `Quyen19492ItemSortingCubeButtons_v2` |

Validation parses the files leniently (trailing commas and comments, which some
shipped D2R layouts use) and checks the strip landed and the native widgets
around it survived.

## Building, the local `deploy/` folder, and Releases

**Toolchain:** MSVC x64 (VS2022 or newer recommended -- an old VS2019 16.11
`cl.exe` is known to ICE on `sort_engine.cpp`'s `BuiltinModes()`, see
"Testing" above), C++20, CMake 3.29+, and the **Ninja** generator rather
than a Visual Studio solution generator (sidesteps CMake/VS-version-name
mismatches, e.g. CMake not yet knowing a brand-new VS release's generator
name). In VS Code's CMake Tools: select an MSVC kit, set
`"cmake.generator": "Ninja"` in settings, pick the Release variant, build.

**Local deploy folder:** every build (any config, VS Code or the command
line) copies the freshly built DLL -- named `d2rl-quyen19492-item-sorting.dll`,
not the CMake target's default `ItemSorting.dll` (see
`OUTPUT_NAME` in `CMakeLists.txt`; required so D2RCore's
"`d2rl-<plugin-id>.mpq` beside the DLL" lookup finds the companion folder
below) -- plus its companion `d2rl-quyen19492-item-sorting.mpq/` resource
folder (the sprites from "Known gaps" #6) into `deploy/d2rloader/plugins/`
(see the `QIS_DEPLOY_PLUGINS_DIR` custom commands in `CMakeLists.txt`). The
whole `deploy/d2rloader` folder can be dragged straight into `<game>/`.
The build also copies the end-user install guide (`packaging/README.md`) to
`deploy/README.md` and `tools/patch_mod_stash_layout.py` to `deploy/`, next to
the `d2rloader` folder, and the default config to `deploy/d2rloader/config/`.
`deploy/` is gitignored -- it is regenerated by every build, not committed.
Optionally set `-DD2RLOADER_GAME_DIR=<path to your D2R install>` (e.g. in a
local, gitignored `CMakeUserPresets.json`) to also have every build copy the
DLL and its companion folder directly into
`<D2RLOADER_GAME_DIR>/d2rloader/plugins/` for live testing, no manual copy
step needed. **If you built this plugin before the DLL rename, delete the
old `Quyen194ItemSorting.dll` (or, after the later id rename,
`d2rl-quyen194-item-sorting.dll` and its `.mpq` folder) from your
`<game>/d2rloader/plugins/`** so it doesn't load twice under two different
filenames.

**GitHub Releases:** `.github/workflows/release.yml` builds Release/x64 and
publishes a GitHub Release automatically, but only when you push a version
tag -- ordinary commits do not trigger it:

```
git tag v0.1.0
git push origin v0.1.0
```

The workflow checks out this repo and
[`D2RLoader/PluginSDK`](https://github.com/D2RLoader/PluginSDK) as sibling
folders (matching the local `D2RLoader-Plugins-Dev/{Quyen19492-Item-Sorting,
PluginSDK}` layout `CMakeLists.txt` expects), builds with the same
Ninja + MSVC combination as above, then zips `deploy/d2rloader`, `deploy/README.md` and
`deploy/patch_mod_stash_layout.py` and attaches the zip to the Release, plus
`packaging/README.md` as a separate asset so the install guide can be read
without downloading. The zip's `d2rloader` folder goes straight into
`<game>/`; its README covers the layout patch step.

How other plugins add UI without touching the mod: RuffnecKk's Remote Stash
button is the same SDK mechanism as this plugin's inventory button -- a
`registerChildLayout` on `StockPanel::PlayerInventory` (its JSON built at
runtime and registered through `ResourceService`); it only computes the
placement by reading the live inventory widgets through native hooks. The
SDK offers child layouts for `PlayerInventory` only, so buttons inside the
stash or cube panels still need the mod-layout patch (or native hooks, which
this plugin does not use).

## Regenerating `item_classification_data.hpp`

Only needed if D2R's item data changes (a new season, a mod). Requires a
checkout of `D2R-Reimagined-Save-Editor` (for its `base/*.txt` files) next
to this plugin:

```
python3 tools/gen_classification.py
```

It reads `../D2R-Reimagined-Save-Editor/base/{itemtypes,weapons,armor,misc,
setitems}.txt` and rewrites `src/item_classification_data.hpp` in place.

The single-grid categories' misc-item table (`src/item_misc_data.hpp`) is
generated separately, straight from a mod's excel folder:

```
python3 tools/gen_misc_classification.py "<game>/mods/Reimagined/Reimagined.mpq/data/global/excel"
```

Its rules are mirrored in `src/mod_item_data.cpp` for `mod_data_path`; keep the
two in sync (the test harness compares them when given the mod folder).
