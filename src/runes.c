#include "runes.h"

#include "palette.h"

#include <string.h>
#include <strings.h>

int rune_find(const char *name)
{
    for (int i = 0; i < RUNE_ART_COUNT; i++)
        if (strcasecmp(name, RUNE_ART[i].name) == 0)
            return i;
    for (int i = 0; i < RUNE_ALIAS_COUNT; i++)
        if (strcasecmp(name, RUNE_ALIASES[i][0]) == 0)
            return rune_find(RUNE_ALIASES[i][1]);
    return RUNE_NONE;
}

const char *rune_name(int i)
{
    return i >= 0 && i < RUNE_ART_COUNT ? RUNE_ART[i].name : "";
}

uint32_t rune_rgb(const uint32_t nord[16], int i)
{
    if (i < 0)
        i = 0;
    return rgb_mix(nord[RUNE_HUE[i % RUNE_HUES]], nord[6], RUNE_PASTEL / 100.0);
}

int rune_pixel(int i, int x, int y)
{
    if (i < 0 || i >= RUNE_ART_COUNT || x < 0 || y < 0 || x >= RUNE_ART_W || y >= RUNE_ART_H)
        return 0;
    return RUNE_ART[i].art[y][x] == '#';
}

int rune_canvas(const uint32_t nord[16], int i, int w, int h, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)w * (size_t)h * 4);
    int s = w / RUNE_ART_W < h / RUNE_ART_H ? w / RUNE_ART_W : h / RUNE_ART_H;
    if (s < 1)
        s = 1;
    int left = (w - RUNE_ART_W * s) / 2, top = (h - RUNE_ART_H * s) / 2;
    uint32_t c = rune_rgb(nord, i);
    for (int y = top < 0 ? 0 : top; y < h && (y - top) / s < RUNE_ART_H; y++)
        for (int x = left < 0 ? 0 : left; x < w && (x - left) / s < RUNE_ART_W; x++) {
            if (!rune_pixel(i, (x - left) / s, (y - top) / s))
                continue;
            uint8_t *p = rgba + 4 * ((size_t)y * (size_t)w + (size_t)x);
            p[0] = (uint8_t)(c >> 16);
            p[1] = (uint8_t)(c >> 8);
            p[2] = (uint8_t)c;
            p[3] = 255;
        }
    return s;
}
