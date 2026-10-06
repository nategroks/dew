#!/usr/bin/env python3
"""The 24 Elder Futhark runes, shared by dew and the slice-rune xfwm4 titlebars.

Each rune is stroke polylines in a 13 x 19 box (x right, y down), after the
"Viking Runes" chart: its glyph, the chart's meaning, a soft Nord color, and a
renderer to a 1-bit sprite of any size. One source, so the runes on a task in
dew and on a window's maximize button are the same pixels.

    tools/runes.py                 print every sprite at the default size
    tools/runes.py --c             write src/runeart.c and src/runeart.h
    tools/runes.py --preview OUT   a PNG sheet of the sprites in their colors

Needs Pillow. The titlebar generator imports this file
(~/.local/share/themes/slice-runes/generate.py).
"""
import math
import os
import sys

# (name, codepoint, meaning, strokes) in futhark order
RUNES = [
    ("fehu", 0x16A0, "wealth", [[(4, 0), (4, 18)], [(4, 7), (10, 1)], [(4, 12), (10, 6)]]),
    ("uruz", 0x16A2, "strength", [[(2, 18), (2, 0), (10, 3.5), (10, 18)]]),
    ("thurisaz", 0x16A6, "giant", [[(4, 0), (4, 18)], [(4, 5), (10, 9.5), (4, 14)]]),
    ("ansuz", 0x16A8, "signals", [[(4, 18), (4, 0), (10, 6)], [(4, 6), (10, 12)]]),
    ("raido", 0x16B1, "journey", [[(3, 18), (3, 0), (9, 4.5), (3, 9)], [(3, 9), (10, 18)]]),
    ("kenaz", 0x16B2, "opening", [[(10, 1), (3, 9.5), (10, 18)]]),
    ("gebo", 0x16B7, "partnership", [[(2, 1), (11, 18)], [(11, 1), (2, 18)]]),
    ("wunjo", 0x16B9, "joy", [[(4, 18), (4, 0), (10, 5.5), (4, 11)]]),
    ("hagalaz", 0x16BA, "disruption", [[(2, 0), (2, 18)], [(11, 0), (11, 18)], [(2, 6), (11, 12)]]),
    ("naudiz", 0x16BE, "need", [[(6, 0), (6, 18)], [(2, 5), (11, 12)]]),
    ("isa", 0x16C1, "ice", [[(6, 0), (6, 18)]]),
    ("jera", 0x16C3, "harvest", [[(3, 0), (8, 5), (3, 10)], [(10, 8), (5, 13), (10, 18)]]),
    ("eihwaz", 0x16C7, "defense", [[(6, 1), (6, 18)], [(6, 1), (10, 5)], [(6, 18), (2, 14)]]),
    ("perthro", 0x16C8, "luck",
     [[(3, 0), (3, 18)], [(3, 0), (7, 4), (10, 1)], [(3, 18), (7, 14), (10, 17)]]),
    ("algiz", 0x16C9, "protection", [[(6, 0), (6, 18)], [(6, 8), (1.5, 2)], [(6, 8), (10.5, 2)]]),
    ("sowilo", 0x16CA, "sun", [[(9, 0), (3, 9)], [(3, 9), (10, 9.5)], [(10, 9.5), (4, 18)]]),
    ("tiwaz", 0x16CF, "the warrior", [[(6, 1), (6, 18)], [(1.5, 6), (6, 1), (10.5, 6)]]),
    ("berkana", 0x16D2, "growth", [[(3, 0), (3, 18)], [(3, 0), (9, 4.5), (3, 9), (9, 13.5), (3, 18)]]),
    ("ehwaz", 0x16D6, "movement", [[(2, 18), (2, 0), (6.5, 6), (11, 0), (11, 18)]]),
    ("mannaz", 0x16D7, "man", [[(2, 18), (2, 0), (11, 10)], [(11, 18), (11, 0), (2, 10)]]),
    ("laguz", 0x16DA, "water", [[(4, 0), (4, 18)], [(4, 0), (10, 6)]]),
    ("ingwaz", 0x16DC, "fertility", [[(3, 0), (10, 9), (3, 18)], [(10, 0), (3, 9), (10, 18)]]),
    ("dagaz", 0x16DE, "day", [[(2, 2), (2, 16), (11, 2), (11, 16), (2, 2)]]),
    ("othala", 0x16DF, "separation", [[(6.5, 0), (11, 5.5), (2, 17)], [(6.5, 0), (2, 5.5), (11, 17)]]),
]
NAMES = [r[0] for r in RUNES]

# Other spellings people write (the chart's among them), for hand-edited task files.
ALIASES = {"urus": "uruz", "thurs": "thurisaz", "os": "ansuz", "raidho": "raido", "kano": "kenaz",
           "kaunan": "kenaz", "wynn": "wunjo", "hagal": "hagalaz", "nauthiz": "naudiz",
           "jeran": "jera", "eiwaz": "eihwaz", "perth": "perthro", "elhaz": "algiz",
           "sowelu": "sowilo", "teiwaz": "tiwaz", "berkano": "berkana", "inguz": "ingwaz",
           "othila": "othala"}

NORD = [0x2e3440, 0x3b4252, 0x434c5e, 0x4c566a, 0xd8dee9, 0xe5e9f0, 0xeceff4, 0x8fbcbb,
        0x88c0d0, 0x81a1c1, 0x5e81ac, 0xbf616a, 0xd08770, 0xebcb8b, 0xa3be8c, 0xb48ead]
# A rune's color: Nord color HUES[i % 8], softened PASTEL percent toward snow (nord6).
# Neighbors in the futhark never share one.
HUES = [13, 14, 7, 8, 9, 15, 12, 11]
PASTEL = 25

W, H = 17, 24      # the shared sprite size (dew's board and the titlebar button)
STROKE = 2.4       # pen width in pixels at W x H
SS = 8             # supersampling


def color(i, nord=NORD):
    """0xRRGGBB of rune i, the way dew computes it (palette.c rgb_mix, rounded)."""
    a, b = nord[HUES[i % len(HUES)]], nord[6]
    out = 0
    for shift in (16, 8, 0):
        x, y = (a >> shift) & 0xFF, (b >> shift) & 0xFF
        v = (y - x) * PASTEL / 100
        v = math.floor(v + 0.5) if v >= 0 else -math.floor(-v + 0.5)  # C lround
        out |= max(0, min(255, x + v)) << shift
    return out


def render(name, w=W, h=H, stroke=None):
    """The rune as rows of '#' and '.', strokes scaled to fill w x h."""
    from PIL import Image, ImageDraw
    strokes = dict((r[0], r[3]) for r in RUNES)[ALIASES.get(name, name)]
    stroke = stroke or STROKE * h / H
    s = min((w - stroke) / 13, (h - stroke) / 18)       # 13 x 18 is the box's span
    ox, oy = (w - 13 * s) / 2, (h - 18 * s) / 2
    im = Image.new("L", (w * SS, h * SS), 0)
    d = ImageDraw.Draw(im)
    r = stroke * SS / 2
    for poly in strokes:
        pts = [((ox + x * s) * SS, (oy + y * s) * SS) for x, y in poly]
        d.line(pts, fill=255, width=round(stroke * SS), joint="curve")
        for px, py in pts:
            d.ellipse((px - r, py - r, px + r, py + r), fill=255)
    small = im.resize((w, h), Image.BOX)
    return ["".join("#" if small.getpixel((x, y)) >= 128 else "." for x in range(w)) for y in range(h)]


def write_c(root):
    hues = ", ".join(str(h) for h in HUES)
    open(os.path.join(root, "src/runeart.h"), "w").write(f"""/* Generated by tools/runes.py. Do not edit. */
#ifndef DEW_RUNEART_H
#define DEW_RUNEART_H

#include <stdint.h>

#define RUNE_ART_W {W}
#define RUNE_ART_H {H}
#define RUNE_ART_COUNT {len(RUNES)}
#define RUNE_ALIAS_COUNT {len(ALIASES)}
#define RUNE_HUES {len(HUES)}
#define RUNE_PASTEL {PASTEL} /* percent toward nord6 */

typedef struct {{
    const char *name, *meaning;
    uint32_t cp;
    const char *art[RUNE_ART_H]; /* '#' ink, '.' clear */
}} RuneArt;

extern const RuneArt RUNE_ART[RUNE_ART_COUNT];        /* futhark order */
extern const char *const RUNE_ALIASES[RUNE_ALIAS_COUNT][2]; /* other spelling, name */
extern const uint8_t RUNE_HUE[RUNE_HUES];             /* rune i is nord[RUNE_HUE[i % RUNE_HUES]] */

#endif
""")
    out = ["/* Generated by tools/runes.py. Do not edit. */", '#include "runeart.h"', "",
           "const RuneArt RUNE_ART[RUNE_ART_COUNT] = {"]
    for name, cp, meaning, _ in RUNES:
        out.append(f'    {{"{name}", "{meaning}", 0x{cp:04X}, {{')
        out += [f'        "{row}",' for row in render(name)]
        out.append("    }},")
    out += ["};", "", "const char *const RUNE_ALIASES[RUNE_ALIAS_COUNT][2] = {"]
    out += [f'    {{"{k}", "{v}"}},' for k, v in ALIASES.items()]
    out += ["};", "", f"const uint8_t RUNE_HUE[RUNE_HUES] = {{{hues}}};", ""]
    open(os.path.join(root, "src/runeart.c"), "w").write("\n".join(out))


def preview(path, scale=6):
    from PIL import Image
    cols = 8
    img = Image.new("RGB", (cols * (W + 3) * scale, 3 * (H + 3) * scale), (0x2E, 0x34, 0x40))
    for i, name in enumerate(NAMES):
        c = color(i)
        ox, oy = (i % cols) * (W + 3) + 1, (i // cols) * (H + 3) + 1
        for y, row in enumerate(render(name)):
            for x, ch in enumerate(row):
                if ch == "#":
                    img.paste((c >> 16, (c >> 8) & 255, c & 255),
                              ((ox + x) * scale, (oy + y) * scale, (ox + x + 1) * scale, (oy + y + 1) * scale))
    img.save(path)


if __name__ == "__main__":
    if "--c" in sys.argv:
        write_c(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    elif "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1])
    else:
        for name in NAMES:
            print(name)
            print("\n".join(render(name)))
            print()
