#include "sprite.h"

#include "palette.h"

#include <string.h>

/* nord[a] mixed pct percent of the way to nord[b] */
typedef struct {
    uint8_t a, b, pct;
} Mix;

typedef struct {
    const char *name;
    uint8_t cycle;
    Mix fur[WOLF_ZONES][4]; /* tail, body, head; snow, frost, shade, deep fur */
    Mix dark, eye, ear;
} Coat;

/* Soft pastel fur around Nord color h, shaded toward x. */
#define FUR(h, x) {{h, 6, 55}, {h, 6, 25}, {h, h, 0}, {h, x, 50}}
#define ALL_ZONES(...) {__VA_ARGS__, __VA_ARGS__, __VA_ARGS__}

static const Coat COATS[WOLF_KINDS] = {
    [WOLF_SNOW] = {"snow", 0, ALL_ZONES({{6, 6, 0}, {4, 4, 0}, {4, 3, 28}, {4, 3, 60}}),
                   {0, 0, 0}, {11, 11, 0}, {15, 15, 0}},
    [WOLF_FROST] = {"frost", 1, ALL_ZONES(FUR(8, 10)), {0, 0, 0}, {10, 10, 0}, {9, 9, 0}},
    [WOLF_EMBER] = {"ember", 0, ALL_ZONES(FUR(12, 11)), {0, 0, 0}, {13, 13, 0}, {11, 11, 0}},
    [WOLF_AURORA] = {"aurora", 1, {FUR(14, 10), FUR(7, 10), FUR(15, 10)},
                     {0, 0, 0}, {13, 13, 0}, {11, 11, 0}},
    [WOLF_SHADOW] = {"shadow", 0, ALL_ZONES({{3, 4, 55}, {3, 4, 40}, {3, 4, 22}, {3, 3, 0}}),
                     {0, 0, 0}, {8, 8, 0}, {10, 10, 0}},
    [WOLF_DUSK] = {"dusk", 1, ALL_ZONES(FUR(15, 10)), {0, 0, 0}, {13, 13, 0}, {11, 11, 0}},
};

static const Coat *coat(int kind)
{
    return &COATS[kind >= 0 && kind < WOLF_KINDS ? kind : WOLF_SNOW];
}

static uint32_t mix(const uint32_t nord[16], Mix m)
{
    return rgb_mix(nord[m.a], nord[m.b], m.pct / 100.0);
}

static int wrap(int frame)
{
    frame %= WOLF_FRAMES;
    return frame < 0 ? frame + WOLF_FRAMES : frame;
}

const char *wolf_name(int kind)
{
    return coat(kind)->name;
}

int wolf_cycle(int kind)
{
    return coat(kind)->cycle;
}

int wolf_pixel(int kind, int frame, int x, int y)
{
    if (x < 0 || y < 0 || x >= WOLF_W || y >= WOLF_H)
        return PX_NONE;
    const char *const *art = WOLF_ART[wolf_cycle(kind)][wrap(frame)];
    switch (art[y * WOLF_ART_H / WOLF_H][x * WOLF_ART_W / WOLF_W] - '0') {
    case ART_SNOW:
    case ART_FROST:
        return PX_SNOW;
    case ART_SHADE:
    case ART_DEEP:
    case ART_EAR:
        return PX_SHADE;
    case ART_EYE:
        return PX_EYE;
    case ART_DARK:
        return PX_DARK;
    default:
        return PX_NONE;
    }
}

uint32_t wolf_rgb(const uint32_t nord[16], int kind, int art, double along)
{
    const Coat *c = coat(kind);
    switch (art) {
    case ART_DARK:
        return mix(nord, c->dark);
    case ART_EYE:
        return mix(nord, c->eye);
    case ART_EAR:
        return mix(nord, c->ear);
    case ART_SNOW:
    case ART_FROST:
    case ART_SHADE:
    case ART_DEEP: {
        int l = art - ART_SNOW;
        along = along < 0 ? 0 : along > 1 ? 1 : along;
        if (along <= 0.5)
            return rgb_mix(mix(nord, c->fur[0][l]), mix(nord, c->fur[1][l]), along * 2);
        return rgb_mix(mix(nord, c->fur[1][l]), mix(nord, c->fur[2][l]), (along - 0.5) * 2);
    }
    default:
        return 0;
    }
}

int wolf_canvas(const uint32_t nord[16], int kind, int frame, int w, int h, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)w * (size_t)h * 4);
    int s = w / WOLF_ART_W < h / WOLF_ART_H ? w / WOLF_ART_W : h / WOLF_ART_H;
    if (s < 1)
        s = 1;
    int left = (w - WOLF_ART_W * s) / 2, top = h - WOLF_ART_H * s;
    const char *const *art = WOLF_ART[wolf_cycle(kind)][wrap(frame)];
    for (int y = top < 0 ? 0 : top; y < h; y++) {
        int ay = (y - top) / s;
        for (int x = left < 0 ? 0 : left; x < w && (x - left) / s < WOLF_ART_W; x++) {
            int ax = (x - left) / s;
            char k = art[ay][ax];
            if (k == '.')
                continue;
            uint32_t c = wolf_rgb(nord, kind, k - '0', (double)ax / (WOLF_ART_W - 1));
            uint8_t *p = rgba + 4 * ((size_t)y * (size_t)w + (size_t)x);
            p[0] = (uint8_t)(c >> 16);
            p[1] = (uint8_t)(c >> 8);
            p[2] = (uint8_t)c;
            p[3] = 255;
        }
    }
    return s;
}

uint32_t rune(int i)
{
    static const uint32_t FUTHARK[RUNE_COUNT] = {
        0x16A0, 0x16A2, 0x16A6, 0x16A8, 0x16B1, 0x16B2, 0x16B7, 0x16B9, /* ᚠᚢᚦᚨᚱᚲᚷᚹ */
        0x16BA, 0x16BE, 0x16C1, 0x16C3, 0x16C7, 0x16C8, 0x16C9, 0x16CA, /* ᚺᚾᛁᛃᛇᛈᛉᛊ */
        0x16CF, 0x16D2, 0x16D6, 0x16D7, 0x16DA, 0x16DC, 0x16DE, 0x16DF, /* ᛏᛒᛖᛗᛚᛜᛞᛟ */
    };
    i %= RUNE_COUNT;
    return FUTHARK[i < 0 ? i + RUNE_COUNT : i];
}
