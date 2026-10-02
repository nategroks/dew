#include "garden.h"
#include "patterns.h"
#include "test.h"

#include <stdlib.h>

static void test_pattern_order(void)
{
    CHECK_STR(garden_pattern_for(1), "beehive");
    CHECK_STR(garden_pattern_for(2), "blinker");
    CHECK_STR(garden_pattern_for(3), "loaf");
    CHECK_STR(garden_pattern_for(4), "penta");
    CHECK_STR(garden_pattern_for(5), "toad");
    CHECK_STR(garden_pattern_for(8), "pulsar");
    CHECK_STR(garden_pattern_for(12), "penta");
    CHECK_STR(garden_pattern_for(16), "pulsar");
}

static void test_no_overlap(void)
{
    Garden g;
    garden_init(&g);
    Rng r;
    rng_seed(&r, 7);
    for (int i = 0; i < 16; i++)
        garden_plant(&g, &r, "09:30");
    CHECK_INT(g.n, 16);
    for (size_t i = 0; i < g.n; i++) {
        const Plant *a = &g.p[i];
        CHECK(a->tint < GARDEN_PAIRS); /* tint is the color pair */
        if (i > 0)
            CHECK(a->tint != g.p[i - 1].tint); /* never the same pair twice in a row */
        if (a->x < 0)
            continue;
        Pattern pa;
        pattern_get(a->pattern, &pa);
        CHECK(a->x >= 3 && a->y >= 3 && a->x + pa.w <= GARDEN_W - 3 && a->y + pa.h <= GARDEN_H - 3);
        for (size_t k = i + 1; k < g.n; k++) {
            const Plant *b = &g.p[k];
            if (b->x < 0)
                continue;
            Pattern pb;
            pattern_get(b->pattern, &pb);
            bool apart = a->x + pa.w + GARDEN_PAD <= b->x || b->x + pb.w + GARDEN_PAD <= a->x ||
                         a->y + pa.h + GARDEN_PAD <= b->y || b->y + pb.h + GARDEN_PAD <= a->y;
            CHECK(apart);
        }
    }
    garden_free(&g);
}

static void test_full_garden_still_records(void)
{
    Garden g;
    garden_init(&g);
    Rng r;
    rng_seed(&r, 3);
    int unplaced = 0;
    for (int i = 0; i < 80; i++)
        unplaced += !garden_plant(&g, &r, "10:00");
    CHECK_INT(g.n, 80); /* every wave is counted even when there's no room */
    CHECK(unplaced > 0);
    garden_free(&g);
}

static void test_round_trip(void)
{
    Garden g, h;
    garden_init(&g);
    garden_init(&h);
    Rng r;
    rng_seed(&r, 11);
    for (int i = 0; i < 5; i++)
        garden_plant(&g, &r, "13:05");
    char *text = garden_serialize(&g);
    garden_parse(&h, text);
    CHECK_INT(h.n, g.n);
    for (size_t i = 0; i < g.n && i < h.n; i++) {
        CHECK_STR(h.p[i].pattern, g.p[i].pattern);
        CHECK_STR(h.p[i].hhmm, "13:05");
        CHECK_INT(h.p[i].x, g.p[i].x);
        CHECK_INT(h.p[i].y, g.p[i].y);
        CHECK_INT(h.p[i].tint, g.p[i].tint);
    }
    free(text);
    garden_free(&h);

    garden_parse(&h, "garbage\n09:00 dragon 1 1 1\n09:00 block 999 1 1\n09:00 block 5 5 9\n"
                     "09:00 block 5 5 2");  /* pair 9 doesn't exist; old files used 1..5 */
    CHECK_INT(h.n, 1); /* only the last line is valid (no trailing newline is fine) */
    garden_free(&h);
    garden_free(&g);
}

static void test_stamp_is_stable(void)
{
    Garden g;
    garden_init(&g);
    Rng r;
    rng_seed(&r, 5);
    for (int i = 0; i < 12; i++)
        garden_plant(&g, &r, "08:00");
    Life l;
    life_init(&l, GARDEN_W, GARDEN_H);
    garden_stamp(&g, &l);
    int pop = life_population(&l);
    CHECK(pop > 0);
    Life copy;
    life_init(&copy, GARDEN_W, GARDEN_H);
    garden_stamp(&g, &copy);
    /* every pattern's period divides 30 (1, 2, 3 for the pulsar, 15 for the penta) */
    for (int i = 0; i < 30; i++)
        life_step(&l);
    int same = 1;
    for (int y = 0; y < GARDEN_H; y++)
        for (int x = 0; x < GARDEN_W; x++)
            same &= life_get(&l, x, y) == life_get(&copy, x, y);
    CHECK(same);
    life_free(&l);
    life_free(&copy);

    /* tiny boards just show fewer plants */
    life_init(&l, 10, 6);
    garden_stamp(&g, &l);
    for (int y = -LIFE_MARGIN; y < 6 + LIFE_MARGIN; y++)
        for (int x = -LIFE_MARGIN; x < 10 + LIFE_MARGIN; x++)
            if (x < 0 || y < 0 || x >= 10 || y >= 6)
                CHECK_INT(life_get(&l, x, y), CELL_OFF);
    life_free(&l);
    garden_free(&g);
}

static void test_layout_and_shade(void)
{
    Garden g;
    garden_init(&g);
    Rng r;
    rng_seed(&r, 21);
    for (int i = 0; i < 10; i++)
        garden_plant(&g, &r, "08:00");
    GardenBox *boxes = NULL;
    size_t n = garden_layout(&g, GARDEN_W, GARDEN_H, &boxes);
    CHECK(n > 0 && n <= g.n);
    Life l;
    life_init(&l, GARDEN_W, GARDEN_H);
    garden_stamp(&g, &l);
    int cells = 0;
    for (size_t i = 0; i < n; i++) {
        CHECK(boxes[i].x >= 1 && boxes[i].y >= 1);
        CHECK(boxes[i].x + boxes[i].w <= GARDEN_W - 1 && boxes[i].y + boxes[i].h <= GARDEN_H - 1);
        CHECK(boxes[i].pair < GARDEN_PAIRS);
        CHECK(boxes[i].period >= 1);
    }
    for (size_t i = 0; i < n; i++)
        for (int y = boxes[i].y; y < boxes[i].y + boxes[i].h; y++)
            for (int x = boxes[i].x; x < boxes[i].x + boxes[i].w; x++)
                cells += life_get(&l, x, y) == CELL_ON;
    CHECK_INT(cells, life_population(&l)); /* every stamped cell lies in a box */

    /* shade: a gradient across the box, always 0..7, moving with time */
    GardenBox still = {10, 10, 4, 3, 2, 1}, osc = {10, 10, 3, 1, 2, 2};
    CHECK(garden_shade(&still, 10, 10, 0) < garden_shade(&still, 13, 12, 0));
    int lo = 7, hi = 0;
    for (long f = 0; f < 400; f += 7)
        for (int y = 5; y < 20; y++)
            for (int x = 5; x < 20; x++) {
                int s = garden_shade(&still, x, y, f);
                CHECK(s >= 0 && s <= 7);
                lo = s < lo ? s : lo;
                hi = s > hi ? s : hi;
            }
    CHECK(lo == 0 && hi == 7);
    CHECK(garden_shade(&osc, 11, 10, 0) != garden_shade(&osc, 11, 10, 8)); /* next generation */
    CHECK(garden_shade(&still, 11, 11, 0) != garden_shade(&still, 11, 11, 40)); /* slow breathing */
    free(boxes);
    life_free(&l);
    garden_free(&g);
}

void suite_garden(void)
{
    test_pattern_order();
    test_no_overlap();
    test_full_garden_still_records();
    test_round_trip();
    test_stamp_is_stable();
    test_layout_and_shade();
}
