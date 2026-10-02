#include "sprite.h"

#include <string.h>

static int wrap(int frame)
{
    frame %= WOLF_FRAMES;
    return frame < 0 ? frame + WOLF_FRAMES : frame;
}

int wolf_pixel(int frame, int x, int y)
{
    if (x < 0 || y < 0 || x >= WOLF_W || y >= WOLF_H)
        return PX_NONE;
    switch (WOLF_ART[wrap(frame)][y * WOLF_ART_H / WOLF_H][x * WOLF_ART_W / WOLF_W] - '0') {
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

int wolf_canvas(int frame, int w, int h, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)w * (size_t)h * 4);
    int s = w / WOLF_ART_W < h / WOLF_ART_H ? w / WOLF_ART_W : h / WOLF_ART_H;
    if (s < 1)
        s = 1;
    int left = (w - WOLF_ART_W * s) / 2, top = h - WOLF_ART_H * s;
    const char *const *art = WOLF_ART[wrap(frame)];
    for (int y = top < 0 ? 0 : top; y < h; y++) {
        int ay = (y - top) / s;
        for (int x = left < 0 ? 0 : left; x < w && (x - left) / s < WOLF_ART_W; x++) {
            char k = art[ay][(x - left) / s];
            if (k == '.')
                continue;
            uint32_t c = WOLF_ART_RGB[k - '0'];
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
