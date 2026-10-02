#include "sprite.h"
#include "test.h"

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
        CHECK_INT(eyes, 1);
        CHECK(snow > 100); /* a solid white body */
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

void suite_sprite(void)
{
    test_wolf_frames();
    test_runes();
}
