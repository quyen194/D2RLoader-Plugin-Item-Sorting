#!/usr/bin/env python3
"""Build the shared-stash icon buttons (Auto Sort / Add Page / Remove Page)
as native D2R SpA1 sprites, HD + .lowend, from the source artwork in
assets/stash_buttons/<name>.png.

Each sprite is one horizontal strip of 4 frames, in the same order as the
native stash page arrows (PANEL\\Stash\\Stash_LeftArrow):
    0 = normal, 1 = hovered, 2 = pressed, 3 = disabled
Only the normal frame is artwork; the other three are derived from it
(brighter + warm glow / darker + slightly smaller / grey + dim).

Usage:
    python gen_stash_button_sprites.py [--src DIR] [--out DIR] [--size PX] [--png]
Defaults: --src assets/stash_buttons, --out the companion MPQ sprite folder,
--size 88 (HD frame edge; .lowend is half). Keep --size in sync with
BUTTON_SIZE in tools/patch_mod_stash_layout.py. Needs Pillow.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps

NAMES = ("autosort", "addpage", "removepage")
REPO = Path(__file__).resolve().parent.parent
DEFAULT_SRC = REPO / "assets" / "stash_buttons"
DEFAULT_OUT = REPO / "d2rl-quyen194-item-sorting.mpq" / "data" / "hd" / "global" / "ui" / "d2rloader" / "quyen194-item-sorting"


def load_master(path: Path) -> Image.Image:
    """Source art cropped to its visible square, alpha cleaned up."""
    img = Image.open(path).convert("RGBA")
    r, g, b, a = img.split()
    # The artwork's body sits at alpha ~251-254; make it fully opaque and drop
    # the faint noise around it so nothing leaks onto the stash background.
    a = a.point(lambda v: 0 if v < 8 else (255 if v > 240 else v))
    img = Image.merge("RGBA", (r, g, b, a))
    box = a.point(lambda v: 255 if v > 10 else 0).getbbox()
    if box is None:
        raise ValueError(f"{path}: image is fully transparent")
    x0, y0, x1, y1 = box
    side = max(x1 - x0, y1 - y0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(img.crop(box), (int(round(side / 2 - (x1 - x0) / 2)), int(round(side / 2 - (y1 - y0) / 2))))
    return square


def rgb_adjust(img: Image.Image, fn) -> Image.Image:
    rgb, alpha = img.convert("RGB"), img.getchannel("A")
    out = fn(rgb).convert("RGBA")
    out.putalpha(alpha)
    return out


def frames_for(master: Image.Image, size: int) -> list[Image.Image]:
    base = master.resize((size, size), Image.LANCZOS)

    hovered = rgb_adjust(base, lambda im: ImageEnhance.Contrast(ImageEnhance.Brightness(im).enhance(1.25)).enhance(1.05))

    small = max(1, round(size * 0.94))
    shrunk = master.resize((small, small), Image.LANCZOS)
    shrunk = rgb_adjust(shrunk, lambda im: ImageEnhance.Brightness(im).enhance(0.78))
    pressed = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    off = (size - small) // 2
    pressed.alpha_composite(shrunk, (off, off + max(1, size // 80)))

    disabled = rgb_adjust(base, lambda im: ImageEnhance.Brightness(ImageOps.grayscale(im).convert("RGB")).enhance(0.6))

    return [base, hovered, pressed, disabled]


def strip(frames: list[Image.Image]) -> Image.Image:
    size = frames[0].width
    out = Image.new("RGBA", (size * len(frames), size), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        out.paste(f, (i * size, 0))
    return out


def write_sprite(path: Path, img: Image.Image, frames: int) -> None:
    img = img.convert("RGBA")
    w, h = img.size
    hdr = bytearray(40)
    hdr[0:4] = b"SpA1"
    struct.pack_into("<HH", hdr, 4, 31, w // frames)
    struct.pack_into("<II", hdr, 8, w, h)
    struct.pack_into("<I", hdr, 20, frames)
    path.write_bytes(bytes(hdr) + img.tobytes())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", type=Path, default=DEFAULT_SRC)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--size", type=int, default=88)
    ap.add_argument("--png", action="store_true", help="also write <name>.png previews of the HD strips")
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    for name in NAMES:
        master = load_master(args.src / f"{name}.png")
        hd = strip(frames_for(master, args.size))
        low = strip(frames_for(master, args.size // 2))
        write_sprite(args.out / f"stash_btn_{name}.sprite", hd, 4)
        write_sprite(args.out / f"stash_btn_{name}.lowend.sprite", low, 4)
        if args.png:
            hd.save(args.out / f"stash_btn_{name}.png")
        print(f"stash_btn_{name}: HD {hd.width}x{hd.height}, lowend {low.width}x{low.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
