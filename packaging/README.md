# Item Sorting (D2RLoader plugin)

Sort buttons for the shared stash, personal stash, inventory and Horadric Cube.
Made for the **Reimagined** mod. Needs **D2RLoader** and **Python 3** (3.8+).

## Install

1. Close the game.
2. Copy the `d2rloader` folder into your game folder (next to `D2R.exe`), merging
   it with the one already there.
3. Add the stash / cube buttons to your mod:

   ```bat
   python patch_mod_stash_layout.py "<game>\mods\Reimagined\Reimagined.mpq"
   ```

   **Run this again after every mod update.**
4. Start the game.

## Uninstall

1. `python patch_mod_stash_layout.py "<game>\mods\Reimagined\Reimagined.mpq" --remove`
2. Delete from `d2rloader`: `plugins/d2rl-quyen19492-item-sorting.dll`, the
   `plugins/d2rl-quyen19492-item-sorting.mpq` folder and
   `config/quyen19492-item-sorting.toml`.

## Upgrading

- Back up `d2rloader/config/quyen19492-item-sorting.toml` before copying; the new
  copy replaces it. Move your values into the new file by hand.

## Buttons

| Where | Button |
| --- | --- |
| Stash, Shared tab | Auto Sort, Add Page, Remove Page |
| Stash, Personal tab | Sort |
| Inventory (bottom right) | Sort -- charms stay where you put them |
| Horadric Cube | Sort |

## Settings

Edit `d2rloader/config/quyen19492-item-sorting.toml`; every key is explained in
the file. Most useful: `shared_stash_tab_count` (set it to your real number of
shared tabs). Changes apply on the next button press.

## Notes

- Multiplayer: works for single player and the TCP/IP host. As a TCP/IP
  **client** nothing is moved (you get a message).
- No stash / cube buttons? Re-run step 3 (`--check` tells you if it's needed).
- Problems? See `d2rloader/logs/quyen19492-item-sorting.log`.
