#include "config.h"
#include "sprite.h"
#include "test.h"
#include "wolfart.h"

#include <stdlib.h>

static void test_wolf_frames(void)
{
    for (int k = 0; k < WOLF_KINDS; k++)
        for (int f = 0; f < WOLF_FRAMES; f++) {
            int eyes = 0, snow = 0;
            for (int y = 0; y < WOLF_H; y++)
                for (int x = 0; x < WOLF_W; x++) {
                    int px = wolf_pixel(k, f, x, y);
                    CHECK(px >= PX_NONE && px <= PX_DARK);
                    eyes += px == PX_EYE;
                    snow += px == PX_SNOW;
                }
            CHECK(eyes >= 1);
            CHECK(snow > 220); /* a solid body */
        }
    CHECK_INT(wolf_pixel(WOLF_SNOW, 0, -1, 0), PX_NONE);
    CHECK_INT(wolf_pixel(WOLF_SNOW, 0, WOLF_W, 0), PX_NONE);
    CHECK_INT(wolf_pixel(WOLF_SNOW, 0, 0, WOLF_H), PX_NONE);
    CHECK_INT(wolf_pixel(WOLF_SNOW, WOLF_FRAMES, 5, 20), wolf_pixel(WOLF_SNOW, 0, 5, 20)); /* frames wrap */
    CHECK_INT(wolf_pixel(-1, 0, 20, 20), wolf_pixel(WOLF_SNOW, 0, 20, 20)); /* unknown kinds are snow */

    /* the legs move: every frame differs from the next in the bottom rows */
    for (int k = 0; k < WOLF_KINDS; k++)
        for (int f = 0; f < WOLF_FRAMES; f++) {
            int diff = 0;
            for (int y = WOLF_H - 5; y < WOLF_H; y++)
                for (int x = 0; x < WOLF_W; x++)
                    diff += wolf_pixel(k, f, x, y) != wolf_pixel(k, f + 1, x, y);
            CHECK(diff > 0);
        }
}

static int shape_diff(int a, int b)
{
    int diff = 0;
    for (int y = 0; y < WOLF_H; y++)
        for (int x = 0; x < WOLF_W; x++)
            diff += (wolf_pixel(a, 0, x, y) != PX_NONE) != (wolf_pixel(b, 0, x, y) != PX_NONE);
    return diff;
}

static void test_wolf_kinds(void)
{
    Config c;
    config_defaults(&c);
    const uint32_t *n = c.nord;
    /* the snow wolf keeps its colors */
    CHECK_INT(wolf_rgb(n, WOLF_SNOW, ART_SNOW, 0.5), 0xeceff4);
    CHECK_INT(wolf_rgb(n, WOLF_SNOW, ART_FROST, 0.5), 0xd8dee9);
    CHECK_INT(wolf_rgb(n, WOLF_SNOW, ART_EYE, 0.5), 0xbf616a);
    CHECK_INT(wolf_rgb(n, WOLF_SNOW, ART_DARK, 0.5), 0x2e3440);

    int gallop = 0, trot = 0;
    for (int a = 0; a < WOLF_KINDS; a++) {
        CHECK(wolf_name(a) != NULL && wolf_name(a)[0]);
        gallop += wolf_cycle(a) == 0;
        trot += wolf_cycle(a) == 1;
        for (int b = 0; b < a; b++) {
            /* every coat is its own color */
            CHECK(wolf_rgb(n, a, ART_SNOW, 0.5) != wolf_rgb(n, b, ART_SNOW, 0.5));
            CHECK(strcmp(wolf_name(a), wolf_name(b)) != 0);
            /* wolves on the same cycle share a shape, the other cycle looks different */
            if (wolf_cycle(a) == wolf_cycle(b))
                CHECK_INT(shape_diff(a, b), 0);
            else
                CHECK(shape_diff(a, b) > 40);
        }
        for (int k = ART_SNOW; k <= ART_EAR; k++)
            CHECK(wolf_rgb(n, a, k, 0.3) != 0);
    }
    CHECK(gallop >= 2 && trot >= 2);
    /* the aurora wolf shifts color from tail to nose; the others don't */
    CHECK(wolf_rgb(n, WOLF_AURORA, ART_SNOW, 0) != wolf_rgb(n, WOLF_AURORA, ART_SNOW, 1));
    CHECK_INT(wolf_rgb(n, WOLF_EMBER, ART_SNOW, 0), wolf_rgb(n, WOLF_EMBER, ART_SNOW, 1));
    /* follows the colors file */
    uint32_t custom[16];
    memcpy(custom, n, sizeof custom);
    custom[6] = 0xffffff;
    CHECK_INT(wolf_rgb(custom, WOLF_SNOW, ART_SNOW, 0.5), 0xffffff);
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
    Config c;
    config_defaults(&c);
    /* scaled by a whole number, bottom-centered, transparent around */
    int w = 100, h = 60;
    uint8_t *px = calloc((size_t)(w * h), 4);
    for (int kind = 0; kind < WOLF_KINDS; kind++) {
        int s = wolf_canvas(c.nord, kind, 2, w, h, px);
        CHECK_INT(s, 2);
        int left = (w - WOLF_ART_W * 2) / 2, top = h - WOLF_ART_H * 2;
        int opaque = 0, match = 0;
        for (int y = 0; y < WOLF_ART_H; y++)
            for (int x = 0; x < WOLF_ART_W; x++) {
                char k = WOLF_ART[wolf_cycle(kind)][2][y][x];
                const uint8_t *p = px + 4 * ((top + 2 * y + 1) * w + left + 2 * x + 1);
                if (k == '.') {
                    CHECK_INT(p[3], 0);
                    continue;
                }
                opaque++;
                uint32_t rgb = wolf_rgb(c.nord, kind, k - '0', (double)x / (WOLF_ART_W - 1));
                match += p[3] == 255 && p[0] == (rgb >> 16) && p[1] == ((rgb >> 8) & 0xFF) &&
                         p[2] == (rgb & 0xFF);
            }
        CHECK(opaque > 200);
        CHECK_INT(match, opaque);
        CHECK_INT(px[3], 0); /* top-left corner */
    }
    free(px);

    /* too small for the art: scale 1, cropped, no overrun (ASan watches) */
    px = calloc(20 * 10, 4);
    CHECK_INT(wolf_canvas(c.nord, WOLF_DUSK, 0, 20, 10, px), 1);
    free(px);
}

void suite_sprite(void)
{
    test_wolf_canvas();
    test_wolf_frames();
    test_wolf_kinds();
    test_runes();
}
