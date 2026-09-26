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
  count.** The SDK has no query for this (see "Known gaps" #3); Auto Sort
  fails cleanly with a toast + log message if it's set too low. A live run
  hit `Result::Unsupported` for a handful of items landing in the two
  highest tabs the padding pass used, which first looked like this value
  being set too high -- but the player confirmed their stash genuinely has
  that many tabs, so the actual cause is still open (see "Known gaps" #3).
  Until it's found, don't assume a too-high value is safe just because this
  run's cause turned out to be something else.
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
- Almost everything in this file (every setting except `enabled`,
  `debug_log_ui_messages`, and `[grid]`) re-reads from disk on every Auto
  Sort run -- edit and save the file, then just run Auto Sort again, no game
  restart needed. See "Known gaps" #14.

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
   `remote-stash-sorting.toml`'s comments for the exact settings.
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

   **Leading hypothesis now:** something specific to the *live application*
   of moves into these particular borrowed/padded-in tabs -- most likely
   that a tab reached only by borrowing (never visited by the player or
   "opened" in the live UI) isn't fully backed/materialized on the game's
   side yet, so `ExistingItemTransaction` rejects a Move *into* it with
   `Unsupported` regardless of what the offline simulation thinks is free
   there. This is a cheap, concrete thing to test directly: before running
   Auto Sort, scroll through and visit every shared-stash tab at least once
   in-game (especially the last few, highest-numbered ones), then run Auto
   Sort again and see whether the same 73-ish items still get rejected. If
   visiting the tabs first makes the rejections disappear, this is a
   real live-only tab-materialization quirk (workaroundable, e.g. by having
   the plugin itself "touch"/select each destination tab before moving
   items into it); if they still fail, the cause is something else again.

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
   `itemtypes/weapons/armor/misc/setitems.txt` -- not from whatever mod is
   actually active in the running game. If a mod's data ever diverges from
   that bundled copy (a future Reimagined patch renumbers `setitems.txt`
   rows again, or you point this plugin at a different mod entirely), the
   embedded table goes stale and Classify()/SetGroupForRow() start lying
   again, silently, exactly like this bug did. There's no SDK-level fix for
   this today (the SDK's `DataTableService` doesn't publish typed row
   structs for these tables either -- see gap #2 above), so as a practical
   mitigation there is now a `mod_data_path` setting (see "Config" above): point it at a mod's
   own folder and the plugin parses that mod's *own* txt files fresh, every
   Auto Sort run, instead of trusting the embedded snapshot. Leave it empty
   to keep using the embedded table (the default, and still the right
   choice if you don't know your mod's data folder path). This was
   validated offline by parsing the same reference txt files this table
   was generated from and diffing every one of the 724 classification
   entries and all 455 `kSetGroupByRow` rows against the embedded table:
   zero mismatches.

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

   **Iteration 5 -- buttons were visible but unclickable; root rect was
   likely clipping hit-testing.** Live test: the strip rendered correctly
   (5 icons visible in the expected spot, confirmed by screenshot), but
   clicking any of the 5 buttons produced no effect at all -- not even
   `LogButtonStripRects`' unconditional debug log line, which only fires
   once `OnUiMessage` sees an event with `target == "remote-stash-sorting"`.
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
   `target="remote-stash-sorting"` line anywhere near it -- so widening the
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
     `WidgetService::dispatchUiAction` with `target="remote-stash-sorting"`,
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
   `"remote-stash-sorting"`: `BC_ASSERT: hashValue != m_hashValues.end()`
   ("Given key not found", `valueName = "remote-stash-sorting"`),
   `BC_ASSERT: channel.IsValid()`, and `BC_ASSERT: false` ("Missing message
   value list registration for %s", `buf = "remote-stash-sorting"`).
   Assertion dialogs are disabled, so D2R just logs and silently continues
   -- no crash, no visible error, nothing in this plugin's own log, nothing
   in the `debug_log_ui_messages` firehose either, because the failure
   happens *before* a UiMessage is ever broadcast. Reading between the
   three messages: a `ButtonWidget.onClickMessage`'s `target` is validated
   against a native, closed registry of "message channels" (`TypeDesc`'s
   "message value list") when the JSON layout is parsed -- built-in names
   like `PanelManager`, `InputMessage`, `MonsterHealth` are pre-registered;
   an arbitrary plugin id like `"remote-stash-sorting"` is not, and never
   becomes a registered channel just by calling
   `SharedEventService::registerUiMessageListener` (that only makes the
   plugin a listener for messages that get broadcast -- it does not
   register the plugin's own id as a valid broadcast target). So every one
   of the five buttons' `onClickMessage` values
   (`"remote-stash-sorting:AutoSort"` etc.) failed this native validation
   silently, every single time, and the click was dropped before it could
   ever produce a `UiMessageEvent` -- fully explaining both symptoms at
   once: the button still shows its native pressed frame (purely a local,
   cursor-driven visual, unrelated to `onClickMessage`), and no
   `target="remote-stash-sorting"` line, and no click-triggered
   `LogButtonStripRects` output, ever appeared in any log, at any rect,
   with or without the root-rect fix.

   The fix: stop inventing a custom `onClickMessage` target entirely.
   `PanelManager` *is* a pre-registered channel (it's how `d2rl-charm-inv`'s
   own button already works: `"PanelManager:TogglePanel:charm-inv/CharmInvPanel"`),
   and `PanelService` exposes `openPanel`/`closePanel`/`togglePanel` as
   direct C++ calls too. So each button now targets
   `"PanelManager:TogglePanel:remote-stash-sorting/Action<Name>"` against
   one dedicated, invisible (`1x1`, no children) trigger panel per action --
   five new panels (`ActionAutoSort`, `ActionAddBlankPage`,
   `ActionRemoveCurrentPage`, `ActionPrevTab`, `ActionNextTab`), registered
   in `D2RLoaderLoadPlugin` alongside the existing `Toast` panel, with
   layout files
   `d2rl-remote-stash-sorting.mpq/data/global/ui/layouts/remote-stash-sorting/Action<Name>hd.json`.
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
    confirmation from an enhanced rejection log added this session. It's
    still a good demonstration of this item's fix working exactly as
    intended even for a failure it wasn't built for: the 73 bad operations
    were isolated and individually logged, and every other operation in
    the same plan still committed normally.

## Testing

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
