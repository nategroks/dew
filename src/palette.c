#include "palette.h"

#include <math.h>

static const uint16_t AGE_EDGES[PAL_AGES] = {0, 1, 2, 4, 8, 16, 30, 60};

int palette_age_bucket(uint16_t age)
{
    int b = 0;
    for (int k = 0; k < PAL_AGES; k++)
        if (age >= AGE_EDGES[k])
            b = k;
    return b;
}

static int ch(uint32_t c, int shift)
{
    return (int)((c >> shift) & 0xff);
}

static uint32_t rgb(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

uint32_t rgb_mix(uint32_t a, uint32_t b, double f)
{
    int r = ch(a, 16) + (int)lround((ch(b, 16) - ch(a, 16)) * f);
    int g = ch(a, 8) + (int)lround((ch(b, 8) - ch(a, 8)) * f);
    int bl = ch(a, 0) + (int)lround((ch(b, 0) - ch(a, 0)) * f);
    return rgb(r, g, bl);
}

/* Newborn -> old: snow, yellow, teal, frost cyan, blue, deep blue, dim gray. */
static uint32_t ramp(const uint32_t n[16], int age)
{
    static const int AT[] = {0, 2, 4, 8, 16, 30, 60};
    const uint32_t col[] = {n[6], n[13], n[7], n[8], n[9], n[10], n[2]};
    const int last = (int)(sizeof AT / sizeof *AT) - 1;
    if (age >= AT[last])
        return col[last];
    int k = 0;
    while (age >= AT[k + 1])
        k++;
    return rgb_mix(col[k], col[k + 1], (double)(age - AT[k]) / (AT[k + 1] - AT[k]));
}

static uint32_t gray_of(uint32_t c)
{
    int l = (int)((0.3 * ch(c, 16) + 0.55 * ch(c, 8) + 0.15 * ch(c, 0)) * 0.45 + 30);
    return rgb(l, l + 4, l + 12);
}

void palette_compute(PaletteRGB *p, const uint32_t n[16])
{
    /* fleets: the five aurora colors, then the four frost ones; they fade in 4 steps */
    const uint32_t tints[LIFE_TINTS] = {0, n[11], n[12], n[13], n[14], n[15], n[7], n[8], n[9], n[10]};
    for (int t = 0; t < LIFE_TINTS; t++)
        for (int b = 0; b < PAL_AGES; b++) {
            uint32_t c = t == 0 ? ramp(n, AGE_EDGES[b]) : rgb_mix(tints[t], n[3], (b / 2) * 0.12);
            p->life[t][b] = c;
            p->gray[t][b] = gray_of(c);
        }
    /* garden: curated aurora/frost gradients */
    static const int PAIRS[GARDEN_PAIRS][2] = {{12, 15}, {14, 7}, {8, 10}, {13, 12}, {15, 9},
                                               {11, 13}, {7, 8},  {9, 15}, {14, 13}};
    for (int g = 0; g < GARDEN_PAIRS; g++)
        for (int s = 0; s < PAL_AGES; s++)
            p->garden[g][s] = rgb_mix(n[PAIRS[g][0]], n[PAIRS[g][1]], s / 7.0);
    p->dying = n[15];
    p->bg = n[0];
    p->band = rgb_mix(n[0], n[1], 0.55);
    p->sel_bg = n[1];
    p->foam[0] = n[6];
    p->foam[1] = n[4];
    p->foam[2] = n[8];
    p->foam[3] = n[7];
    p->glyph[0] = n[6];
    p->glyph[1] = n[8];
    p->glyph[2] = n[7];
    p->glyph[3] = n[9];
    p->glyph[4] = n[13];
    p->ui[UI_TEXT] = n[4];
    p->ui[UI_DIM] = rgb_mix(n[3], n[4], 0.35);
    p->ui[UI_LABEL] = n[8];
    p->ui[UI_SEL] = n[8];
    p->ui[UI_ACTIVE] = n[13];
    p->ui[UI_NOTE] = n[15];
    p->ui[UI_COUNT] = n[7];
    p->ui[UI_BORDER] = n[3];
    p->ui[UI_WARN] = n[11];
}

static int dist2(uint32_t a, uint32_t b)
{
    int dr = ch(a, 16) - ch(b, 16), dg = ch(a, 8) - ch(b, 8), db = ch(a, 0) - ch(b, 0);
    return dr * dr + dg * dg + db * db;
}

int palette_nearest256(uint32_t c)
{
    static const int LEVEL[6] = {0, 95, 135, 175, 215, 255};
    int idx[3];
    for (int k = 0; k < 3; k++) {
        int v = ch(c, 16 - 8 * k), best = 0;
        for (int i = 1; i < 6; i++)
            if ((v - LEVEL[i]) * (v - LEVEL[i]) < (v - LEVEL[best]) * (v - LEVEL[best]))
                best = i;
        idx[k] = best;
    }
    int cube = 16 + 36 * idx[0] + 6 * idx[1] + idx[2];
    uint32_t cube_rgb = rgb(LEVEL[idx[0]], LEVEL[idx[1]], LEVEL[idx[2]]);

    int avg = (ch(c, 16) + ch(c, 8) + ch(c, 0)) / 3;
    int g = (avg - 8 + 5) / 10;
    g = g < 0 ? 0 : g > 23 ? 23 : g;
    int gv = 8 + 10 * g;
    uint32_t gray_rgb = rgb(gv, gv, gv);
    return dist2(c, gray_rgb) < dist2(c, cube_rgb) ? 232 + g : cube;
}

int palette_nearest8(uint32_t c)
{
    /* curses order: black red green yellow blue magenta cyan white */
    int r = ch(c, 16) > 127, g = ch(c, 8) > 127, b = ch(c, 0) > 127;
    return r | g << 1 | b << 2;
}
