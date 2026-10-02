#include "sprite.h"
#include "test.h"
#include "wolfart.h"

#include <stdlib.h>

static void test_wolf_frames(void)
{
    for (int f = 0; f < WOLF_FRAMES; f++) {
        int eyes = 0, snow = 0;
        for (int y = 0; y < WOLF_H; y++)
            for (int x = 0; x < WOLF_W; x++) {
                int px = wolf_pixel(f, x, y);
                CHECK(px >= PX_NONE && px <= PX_DARK);
                eyes += px == PX_EYE;
                snow += px == PX_SNOW;
            }
        CHECK(eyes >= 1);
        CHECK(snow > 300); /* a solid white body */
    }
    CHECK_INT(wolf_pixel(0, -1, 0), PX_NONE);
    CHECK_INT(wolf_pixel(0, WOLF_W, 0), PX_NONE);
    CHECK_INT(wolf_pixel(0, 0, WOLF_H), PX_NONE);
    CHECK_INT(wolf_pixel(WOLF_FRAMES, 5, 5), wolf_pixel(0, 5, 5)); /* frames wrap */

    /* the legs move: every frame differs from the next in the bottom rows */
    for (int f = 0; f < WOLF_FRAMES; f++) {
        int diff = 0;
        for (int y = WOLF_H - 5; y < WOLF_H; y++)
            for (int x = 0; x < WOLF_W; x++)
                diff += wolf_pixel(f, x, y) != wolf_pixel(f + 1, x, y);
        CHECK(diff > 0);
    }
}

static void test_runes(void)
{
    CHECK_INT(rune(0), 0x16A0); /* fehu */
    CHECK_INT(rune(RUNE_COUNT), rune(0));
    CHECK_INT(rune(-1), rune(RUNE_COUNT - 1));
    for (int i = 0; i < RUNE_COUNT; i++) {
        CHECK(rune(i) >= 0x16A0 && rune(i) <= 0x16F8);
        for (int k = 0; k < i; k++)
            CHECK(rune(i) != rune(k));
    }
}

static void test_wolf_canvas(void)
{
    /* scaled by a whole number, bottom-centered, transparent around */
    int w = 100, h = 60;
    uint8_t *px = calloc((size_t)(w * h), 4);
    int s = wolf_canvas(2, w, h, px);
    CHECK_INT(s, 2);
    int left = (w - WOLF_ART_W * 2) / 2, top = h - WOLF_ART_H * 2;
    int opaque = 0, match = 0;
    for (int y = 0; y < WOLF_ART_H; y++)
        for (int x = 0; x < WOLF_ART_W; x++) {
            char k = WOLF_ART[2][y][x];
            const uint8_t *p = px + 4 * ((top + 2 * y + 1) * w + left + 2 * x + 1);
            if (k == '.') {
                CHECK_INT(p[3], 0);
                continue;
            }
            opaque++;
            uint32_t rgb = WOLF_ART_RGB[k - '0'];
            match += p[3] == 255 && p[0] == (rgb >> 16) && p[1] == ((rgb >> 8) & 0xFF) &&
                     p[2] == (rgb & 0xFF);
        }
    CHECK(opaque > 200);
    CHECK_INT(match, opaque);
    CHECK_INT(px[3], 0); /* top-left corner */
    free(px);

    /* too small for the art: scale 1, cropped, no overrun (ASan watches) */
    px = calloc(20 * 10, 4);
    CHECK_INT(wolf_canvas(0, 20, 10, px), 1);
    free(px);
}

void suite_sprite(void)
{
    test_wolf_canvas();
    test_wolf_frames();
    test_runes();
}
