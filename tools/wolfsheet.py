#!/usr/bin/env python3
"""Cut dew's wolf run cycle out of the pixel-art sheet and write src/wolfart.c.

    tools/wolfsheet.py SHEET.png [--preview out.png]

The sheet is a screenshot of upscaled pixel art (about 4 screen pixels per art
pixel) of white wolves running right over a forest and river. Each frame is cut
from its box below, separated from the background by color (light, unsaturated
fur; red eyes and ears; the black mouth and nose next to the fur), reduced to
art resolution and snapped to a small Nord palette. Needs Pillow.
"""
import colorsys
import sys
from collections import deque

from PIL import Image

# Screen-pixel boxes (x0, y0, x1, y1) of the poses, in run-cycle order.
FRAMES = [
    (237, 384, 391, 454),  # stretched low
    (239, 587, 392, 672),  # reaching
    (407, 592, 562, 681),  # gathering
    (230, 467, 388, 563),  # collected
    (408, 359, 562, 451),  # pushing off
    (411, 141, 561, 232),  # flying
]
CELL = 4  # screen pixels per art pixel

# Palette: index -> (name, rgb). 0 is transparent.
PALETTE = [
    ("none", None),
    ("snow", (0xEC, 0xEF, 0xF4)),     # nord6
    ("frost", (0xD8, 0xDE, 0xE9)),    # nord4
    ("shade", (0xAE, 0xB8, 0xC9)),    # nord4 toward nord3
    ("deep", (0x81, 0x8C, 0xA3)),     # nord3 lifted
    ("dark", (0x2E, 0x34, 0x40)),     # nord0: mouth, nose
    ("eye", (0xBF, 0x61, 0x6A)),      # nord11
    ("ear", (0xB4, 0x8E, 0xAD)),      # nord15
]


def hsv(c):
    return colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)


def is_fur(c):
    h, s, v = hsv(c)
    return v > 0.55 and s < 0.22


def is_red(c):
    r, g, b = c
    return r > 110 and r > 1.6 * g and r > 1.4 * b


def is_dark(c):
    h, s, v = hsv(c)
    return v < 0.22 and s < 0.45


def cut(im, box):
    x0, y0, x1, y1 = box
    crop = im.crop((x0 - 6, y0 - 6, x1 + 6, y1 + 6))
    w, h = crop.size
    px = crop.load()
    fur = [[is_fur(px[x, y]) for x in range(w)] for y in range(h)]
    near = [[any(fur[v][u] for u in range(max(0, x - 4), min(w, x + 5))
                 for v in range(max(0, y - 4), min(h, y + 5)))
             for x in range(w)] for y in range(h)]
    fx = [x for y in range(h) for x in range(w) if fur[y][x]]
    fy = [y for y in range(h) for x in range(w) if fur[y][x]]
    # mouth and nose live in the front third, above the legs
    head = min(fx) + (max(fx) - min(fx)) * 3 // 4
    chin = min(fy) + (max(fy) - min(fy)) * 3 // 5
    keep = [[fur[y][x] or (near[y][x] and (is_red(px[x, y]) or
                                           (x >= head and y <= chin and is_dark(px[x, y]))))
             for x in range(w)] for y in range(h)]
    # largest 8-connected blob: the wolf, without stray runes or highlights
    seen = [[False] * w for _ in range(h)]
    best = []
    for y in range(h):
        for x in range(w):
            if keep[y][x] and not seen[y][x]:
                q, blob = deque([(x, y)]), []
                seen[y][x] = True
                while q:
                    a, b = q.popleft()
                    blob.append((a, b))
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            u, v = a + dx, b + dy
                            if 0 <= u < w and 0 <= v < h and keep[v][u] and not seen[v][u]:
                                seen[v][u] = True
                                q.append((u, v))
                if len(blob) > len(best):
                    best = blob
    mask = [[False] * w for _ in range(h)]
    for a, b in best:
        mask[b][a] = True
    return crop, mask


def classify(c):
    if is_red(c):
        r, g, b = c
        return 6 if g < 0.45 * r and b < 0.5 * r else 7  # deep red eye, mauve ear
    if is_dark(c):
        return 5
    v = hsv(c)[2]
    return 1 if v > 0.90 else 2 if v > 0.80 else 3 if v > 0.66 else 4


def reduce(crop, mask):
    w, h = crop.size
    px = crop.load()
    xs = [x for y in range(h) for x in range(w) if mask[y][x]]
    ys = [y for y in range(h) for x in range(w) if mask[y][x]]
    x0, y0 = min(xs), min(ys)
    aw, ah = (max(xs) - x0) // CELL + 1, (max(ys) - y0) // CELL + 1
    art = []
    for ay in range(ah):
        row = []
        for ax in range(aw):
            votes = {}
            n = 0
            for y in range(y0 + ay * CELL, min(h, y0 + (ay + 1) * CELL)):
                for x in range(x0 + ax * CELL, min(w, x0 + (ax + 1) * CELL)):
                    if mask[y][x]:
                        n += 1
                        k = classify(px[x, y])
                        votes[k] = votes.get(k, 0) + 1
            if n * 2 < CELL * CELL:
                row.append(0)
            else:
                # eyes and mouth are small: let them win with fewer votes
                k = max(votes, key=lambda k: votes[k] * (3 if k in (5, 6) else 1))
                row.append(k)
        art.append(row)
    return art


def canvas(arts):
    """Same-size frames, aligned on the ground line and the nose."""
    W = max(len(a[0]) for a in arts)
    H = max(len(a) for a in arts)
    out = []
    for a in arts:
        h, w = len(a), len(a[0])
        grid = [[0] * W for _ in range(H)]
        for y in range(h):
            for x in range(w):
                grid[H - h + y][W - w + x] = a[y][x]
        out.append(grid)
    return out


def write_c(frames, path):
    H, W = len(frames[0]), len(frames[0][0])
    lines = [
        "/* Generated by tools/wolfsheet.py from the wolf pixel-art sheet. Do not edit. */",
        '#include "wolfart.h"',
        "",
        "const uint32_t WOLF_ART_RGB[WOLF_ART_COLORS] = {",
        "    0x000000, /* transparent */",
    ]
    for name, rgb in PALETTE[1:]:
        lines.append("    0x%02X%02X%02X, /* %s */" % (rgb + (name,)))
    lines += ["};", "", "/* '.' is transparent, digits index WOLF_ART_RGB. */",
              "const char *const WOLF_ART[WOLF_ART_FRAMES][WOLF_ART_H] = {"]
    for f in frames:
        lines.append("    {")
        for row in f:
            lines.append('        "%s",' % "".join("." if k == 0 else str(k) for k in row))
        lines.append("    },")
    lines.append("};")
    open(path, "w").write("\n".join(lines) + "\n")
    hdr = path[:-2] + ".h"
    open(hdr, "w").write(f"""/* Generated by tools/wolfsheet.py. Do not edit. */
#ifndef DEW_WOLFART_H
#define DEW_WOLFART_H

#include <stdint.h>

#define WOLF_ART_W {W}
#define WOLF_ART_H {H}
#define WOLF_ART_FRAMES {len(frames)}
#define WOLF_ART_COLORS {len(PALETTE)}

enum {{ ART_NONE, ART_SNOW, ART_FROST, ART_SHADE, ART_DEEP, ART_DARK, ART_EYE, ART_EAR }};

extern const uint32_t WOLF_ART_RGB[WOLF_ART_COLORS];
extern const char *const WOLF_ART[WOLF_ART_FRAMES][WOLF_ART_H];

#endif
""")


def preview(frames, path, s=8):
    H, W = len(frames[0]), len(frames[0][0])
    img = Image.new("RGB", ((W + 2) * s * len(frames), (H + 2) * s), (0x3B, 0x42, 0x52))
    for i, f in enumerate(frames):
        for y, row in enumerate(f):
            for x, k in enumerate(row):
                if k:
                    img.paste(PALETTE[k][1], (((i * (W + 2)) + 1 + x) * s, (1 + y) * s,
                                              ((i * (W + 2)) + 2 + x) * s, (2 + y) * s))
    img.save(path)


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    im = Image.open(args[0]).convert("RGB")
    frames = canvas([reduce(*cut(im, b)) for b in FRAMES])
    if "--preview" in args:
        preview(frames, args[args.index("--preview") + 1])
    else:
        write_c(frames, "src/wolfart.c")
    print("%d frames, %dx%d" % (len(frames), len(frames[0][0]), len(frames[0])))


if __name__ == "__main__":
    main()
