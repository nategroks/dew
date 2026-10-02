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
        CHECK_INT(a->tint, 1 + (int)(i + 1) % 5);
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
                     "09:00 block 5 5 2");
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

void suite_garden(void)
{
    test_pattern_order();
    test_no_overlap();
    test_full_garden_still_records();
    test_round_trip();
    test_stamp_is_stable();
}
