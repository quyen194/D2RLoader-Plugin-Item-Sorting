# Item Sorting (D2RLoader plugin) -- installation

Adds sorting buttons to Diablo II: Resurrected through D2RLoader:

| Where | Button | What it does |
| --- | --- | --- |
| Stash, **Shared** tab | Auto Sort, Add Page, Remove Page | sorts the whole shared stash by category, one or more tabs per category; inserts / removes an empty page |
| Stash, **Personal** tab | Sort | sorts the personal stash |
| Inventory panel (bottom right) | Sort | sorts the inventory (charms stay where you put them by default) |
| Horadric Cube panel, and the stash's cube tab | Sort | sorts the cube |

Built and tested for the **Reimagined** mod. Other mods work if their stash and
cube layouts have the same widgets (the patch script tells you if not).

## Requirements

- Diablo II: Resurrected with **D2RLoader** installed and working.
- **Python 3** (any 3.8+; nothing to `pip install`) -- only for the one-time
  layout patch in step 3.

## What's in this package

```
d2rloader/
  plugins/d2rl-quyen19492-item-sorting.dll      the plugin
  plugins/d2rl-quyen19492-item-sorting.mpq/     its resources (button icons, texts, layouts)
  config/quyen19492-item-sorting.toml           default settings
patch_mod_stash_layout.py                     adds the stash / cube buttons to your mod
README.md                                     this file
```

## Install

1. **Close the game.**
2. **Copy the `d2rloader` folder into your game folder** (the one containing
   `D2R.exe`), merging it with the `d2rloader` folder already there.
   - Already have a `d2rloader/config/quyen19492-item-sorting.toml` from an
     earlier version? Back it up first -- this copy replaces it. Then move your
     own values into the new file (section names changed in this version:
     everything is under `[item-sorting]`, `[item-sorting.shared_stash]`,
     `[item-sorting.personal_stash]`, `[item-sorting.inventory]`,
     `[item-sorting.cube]`; an old-style file is ignored and the plugin logs
     a warning).
   - Upgrading from the old `remote-stash-sorting` plugin? Delete
     `d2rl-remote-stash-sorting.dll`, the `d2rl-remote-stash-sorting.mpq`
     folder and `remote-stash-sorting.toml`, or both plugins load.
   - Upgrading from `quyen194-item-sorting` (the previous name of this
     plugin)? Delete `d2rl-quyen194-item-sorting.dll`, the
     `d2rl-quyen194-item-sorting.mpq` folder and
     `config/quyen194-item-sorting.toml` (copy your settings into
     `quyen19492-item-sorting.toml` first), then re-run step 3 -- the old
     buttons point at the old name and stop working until it is re-run.
3. **Add the stash / cube buttons to your mod** (the inventory button needs
   nothing -- the plugin adds it by itself). D2RLoader plugins cannot add
   buttons to the stash or cube panels, but your mod's own layout files can,
   so this script inserts them there. Run it on your mod's `.mpq` **folder**:

   ```
   python patch_mod_stash_layout.py "<game>\mods\Reimagined\Reimagined.mpq"
   ```

   It edits `data\global\ui\layouts\bankexpansionlayouthd.json` and
   `horadriccubelayouthd.json` inside that folder. Every file is copied to
   `.bak`, `.bak1`, ... before it is changed (existing backups are never
   overwritten), and the result is checked before it is written.

   Other options:

   ```
   python patch_mod_stash_layout.py "<mod.mpq folder>" --check    # report only (exit 0 = up to date)
   python patch_mod_stash_layout.py "<mod.mpq folder>" --remove   # take every button out again
   ```

   **Run it again after every mod update** -- the mod's installer replaces
   those layout files, which removes the buttons. Running it on an
   already-patched mod changes nothing, and it upgrades buttons written by an
   older version of the script.
4. Start the game.

## Settings

Edit `d2rloader/config/quyen19492-item-sorting.toml`. Every setting is explained
in the file itself.

- `[item-sorting]` -- plugin-wide (`enabled` switches the whole plugin off).
  Read once when the game starts.
- `[item-sorting.shared_stash]`, `.personal_stash`, `.inventory`, `.cube` --
  one section per container, each with its own `enabled` and its own category
  list. Re-read every time you press a button, so you can edit and save the
  file while playing.

The settings you are most likely to change:

- `shared_stash_tab_count` in `[item-sorting.shared_stash]` -- set it to the
  number of shared-stash tabs you really have.
- `add_page_enabled` / `remove_page_enabled` -- show or hide the Add Page /
  Remove Page buttons.
- `keep_in_place` in `[item-sorting.inventory]` -- charms stay put by default.
- `grid_width` / `grid_height` -- only if your mod's containers are not the
  Reimagined sizes (shared and personal stash 16 x 13, inventory 10 x 8,
  cube 12 x 8).

## Multiplayer

Sorting works when you host (single player, or the host of a TCP/IP game). As
a TCP/IP **client** the game does not allow the plugin to move items; you get
a message saying so and nothing is moved.

## Troubleshooting

- The plugin writes `d2rloader/logs/quyen19492-item-sorting.log`. Every button
  press logs what it read, what it planned and what happened, and every
  config problem is listed there with the exact key.
- **No buttons in the stash or cube:** step 3 was not run, or the mod was
  updated since. Run `patch_mod_stash_layout.py --check`.
- **No button in the inventory:** look for `inventory Sort button registered`
  in the log. Another inventory plugin can cover it; the position is in
  `d2rloader/plugins/d2rl-quyen19492-item-sorting.mpq/data/global/ui/layouts/quyen19492-item-sorting/InventoryToolshd.json`.
- **"invalid settings" message:** the log names the key and the problem.
- A sort that cannot be done safely moves nothing at all.

## Uninstall

1. `python patch_mod_stash_layout.py "<mod.mpq folder>" --remove`
2. Delete `d2rloader/plugins/d2rl-quyen19492-item-sorting.dll`, the
   `d2rloader/plugins/d2rl-quyen19492-item-sorting.mpq` folder and
   `d2rloader/config/quyen19492-item-sorting.toml`.
