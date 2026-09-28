#!/usr/bin/env python3
"""Generate the shared-stash icon buttons (Auto Sort / Add Page / Remove Page)
as native D2R SpA1 sprites, HD + .lowend.

Each sprite is one horizontal strip of 4 frames, in the same order as the
native stash page arrows (PANEL\\Stash\\Stash_LeftArrow):
    0 = normal, 1 = hovered, 2 = pressed, 3 = disabled
The look follows the native gold-withdraw button (goldbutton.sprite): a bevelled
steel frame around a dark inset, with a gold glyph.

Usage:  python gen_stash_button_sprites.py <output dir> [--png]
Needs Pillow. Output: stash_btn_<name>.sprite / .lowend.sprite (+ previews with --png).
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

FRAME = 72          # HD frame size in px (native gold button is 72x74)
SS = 4              # supersampling factor
NAMES = ("autosort", "addpage", "removepage")


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def vgradient(size, top, bottom):
    w, h = size
    img = Image.new("RGBA", size)
    px = img.load()
    for y in range(h):
        c = lerp(top, bottom, y / max(1, h - 1))
        for x in range(w):
            px[x, y] = c
    return img


def glyph_mask(name: str, s: int) -> Image.Image:
    """White-on-black mask of the icon, drawn on an s x s canvas."""
    m = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(m)
    c = s / 2
    if name == "addpage":
        t, l = s * 0.12, s * 0.30
        d.rounded_rectangle((c - l, c - t / 2, c + l, c + t / 2), radius=t * 0.3, fill=255)
        d.rounded_rectangle((c - t / 2, c - l, c + t / 2, c + l), radius=t * 0.3, fill=255)
    elif name == "removepage":
        t, l = s * 0.12, s * 0.30
        d.rounded_rectangle((c - l, c - t / 2, c + l, c + t / 2), radius=t * 0.3, fill=255)
    elif name == "autosort":
        # three bars, longest on top, plus a down arrow on the right
        t = s * 0.085
        x0 = s * 0.20
        for i, frac in enumerate((0.40, 0.29, 0.18)):
            y = s * 0.30 + i * s * 0.17
            d.rounded_rectangle((x0, y, x0 + s * frac, y + t), radius=t * 0.3, fill=255)
        ax = s * 0.72
        shaft = s * 0.075
        d.rectangle((ax - shaft / 2, s * 0.26, ax + shaft / 2, s * 0.60), fill=255)
        d.polygon([(ax - s * 0.13, s * 0.56), (ax + s * 0.13, s * 0.56), (ax, s * 0.76)], fill=255)
    return m


def render_frame(name: str, state: str) -> Image.Image:
    s = FRAME * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))

    # --- steel bevel frame ---------------------------------------------
    outer = vgradient((s, s), (150, 150, 152, 255), (58, 58, 62, 255))
    rim = int(s * 0.10)
    frame_mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(frame_mask).rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.06), fill=255)
    img.paste(outer, (0, 0), frame_mask)
    d = ImageDraw.Draw(img)
    # dark outline + highlight lines for the bevel
    d.rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.06), outline=(18, 18, 20, 255), width=SS * 2)
    d.line((SS * 3, SS * 3, s - SS * 4, SS * 3), fill=(205, 205, 208, 255), width=SS)
    d.line((SS * 3, SS * 3, SS * 3, s - SS * 4), fill=(185, 185, 188, 255), width=SS)

    # --- inset field ----------------------------------------------------
    inset = (rim, rim, s - rim - 1, s - rim - 1)
    field_top, field_bottom = {
        "normal": ((52, 40, 24, 255), (22, 16, 10, 255)),
        "hovered": ((88, 66, 34, 255), (38, 28, 14, 255)),
        "pressed": ((30, 22, 13, 255), (14, 10, 6, 255)),
        "disabled": ((40, 40, 40, 255), (20, 20, 20, 255)),
    }[state]
    fw = inset[2] - inset[0] + 1
    field = vgradient((fw, fw), field_top, field_bottom)
    img.paste(field, (inset[0], inset[1]))
    d.rectangle(inset, outline=(10, 8, 6, 255), width=SS * 2)
    # inner shadow at top-left of the inset
    d.line((inset[0] + SS * 2, inset[1] + SS * 2, inset[2] - SS * 2, inset[1] + SS * 2), fill=(0, 0, 0, 160), width=SS * 2)
    d.line((inset[0] + SS * 2, inset[1] + SS * 2, inset[0] + SS * 2, inset[3] - SS * 2), fill=(0, 0, 0, 160), width=SS * 2)

    # --- glyph ------------------------------------------------------------
    gs = fw
    mask = glyph_mask(name, gs)
    off = SS * 2 if state == "pressed" else 0
    gold_top, gold_bottom = {
        "normal": ((250, 222, 150, 255), (182, 128, 42, 255)),
        "hovered": ((255, 244, 196, 255), (226, 170, 70, 255)),
        "pressed": ((214, 184, 118, 255), (150, 102, 32, 255)),
        "disabled": ((150, 150, 150, 255), (95, 95, 95, 255)),
    }[state]
    gold = vgradient((gs, gs), gold_top, gold_bottom)

    # drop shadow + dark outline so the glyph reads on the dark field
    outline = mask.filter(ImageFilter.MaxFilter(SS * 2 + 1))
    shadow = ImageChops.offset(outline, SS * 2, SS * 2).filter(ImageFilter.GaussianBlur(SS * 1.5))
    black = Image.new("RGBA", (gs, gs), (0, 0, 0, 255))
    layer = Image.new("RGBA", (gs, gs), (0, 0, 0, 0))
    layer.paste(black, (0, 0), shadow.point(lambda v: v * 0.8))
    layer.paste(Image.new("RGBA", (gs, gs), (20, 12, 4, 255)), (0, 0), outline)
    layer.paste(gold, (0, 0), mask)
    if state == "hovered":
        glow = mask.filter(ImageFilter.GaussianBlur(SS * 3)).point(lambda v: v * 0.6)
        g = Image.new("RGBA", (gs, gs), (255, 200, 90, 0))
        g.putalpha(glow)
        layer = Image.alpha_composite(g, layer)
    img.alpha_composite(layer, (inset[0] + off, inset[1] + off))

    return img.resize((FRAME, FRAME), Image.LANCZOS)


def strip(name: str) -> Image.Image:
    frames = [render_frame(name, st) for st in ("normal", "hovered", "pressed", "disabled")]
    out = Image.new("RGBA", (FRAME * 4, FRAME), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        out.paste(f, (i * FRAME, 0))
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
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for name in NAMES:
        hd = strip(name)
        write_sprite(out / f"stash_btn_{name}.sprite", hd, 4)
        low = hd.resize((hd.width // 2, hd.height // 2), Image.LANCZOS)
        write_sprite(out / f"stash_btn_{name}.lowend.sprite", low, 4)
        if "--png" in sys.argv:
            hd.save(out / f"stash_btn_{name}.png")
        print(f"wrote stash_btn_{name}.sprite ({hd.width}x{hd.height}) + .lowend ({low.width}x{low.height})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
