#include "director.h"
#include "sprite.h"
#include "test.h"

#include <stdlib.h>

static void small_garden(Garden *g)
{
    garden_init(g);
    Rng r;
    rng_seed(&r, 99);
    for (int i = 0; i < 3; i++)
        garden_plant(g, &r, "09:00");
}

static bool same_board(const Life *a, const Life *b)
{
    if (a->w != b->w || a->h != b->h)
        return false;
    for (int y = 0; y < a->h; y++)
        for (int x = 0; x < a->w; x++)
            if (life_get(a, x, y) != life_get(b, x, y))
                return false;
    return true;
}

static void run(Director *d, int frames)
{
    for (int i = 0; i < frames; i++)
        director_frame(d);
}

static void test_idle_shows_garden(void)
{
    Garden g;
    small_garden(&g);
    Director d;
    director_init(&d, 96, 48, 1, &g);
    Life ref;
    life_init(&ref, 96, 48);
    garden_stamp(&g, &ref);
    CHECK(life_population(&d.life) > 0);
    CHECK(same_board(&d.life, &ref));
    CHECK(!director_animating(&d));
    GardenBox *boxes = NULL;
    CHECK_INT(d.nboxes, garden_layout(&g, 96, 48, &boxes));
    free(boxes);
    life_free(&ref);
    director_free(&d);
    garden_free(&g);
}

static void test_wave_start_seeds_a_band(void)
{
    Director d;
    director_init(&d, 96, 48, 2, NULL);
    director_on_wave_start(&d);
    CHECK_INT(d.mood, MOOD_FOCUS);
    CHECK(d.tear > 0);
    CHECK(life_population(&d.life) > 50);
    int top = 48, bottom = -1;
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 96; x++)
            if (life_get(&d.life, x, y)) {
                top = y < top ? y : top;
                bottom = y > bottom ? y : bottom;
            }
    CHECK(bottom - top < 20);
    CHECK(director_animating(&d));
    /* seeded in patches of several soft colors, not one */
    int seen[LIFE_TINTS] = {0}, colors = 0;
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 96; x++)
            if (life_get(&d.life, x, y))
                seen[life_tint(&d.life, x, y)] = 1;
    for (int t = 0; t < LIFE_TINTS; t++)
        colors += seen[t];
    CHECK(colors >= 3);
    director_free(&d);
}

static void test_fleet_color(void)
{
    Director d;
    director_init(&d, 96, 48, 3, NULL);
    director_on_task_done(&d);
    int pop = life_population(&d.life);
    CHECK(pop > 0);
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 96; x++)
            if (life_get(&d.life, x, y))
                CHECK_INT(life_tint(&d.life, x, y), 1);
    CHECK(d.excite > 0); /* idle: run fast for a while */
    run(&d, 200);
    CHECK_INT(d.excite, 0); /* then the garden comes back */
    director_on_task_done(&d);
    CHECK_INT(d.tint_cursor, 2); /* next fleet, next color */
    director_free(&d);
}

static void test_finish_break_cycle(void)
{
    Garden g;
    small_garden(&g);
    Director d;
    director_init(&d, 96, 48, 4, &g);
    director_on_wave_start(&d);
    run(&d, 30);
    director_on_wave_finish(&d);
    CHECK_INT(d.mood, MOOD_BREAK);
    CHECK(!d.sweeping);
    run(&d, 40);
    CHECK(d.sweeping);
    int guard = 0;
    while (d.sweeping && guard++ < 500)
        director_frame(&d);
    CHECK(!d.sweeping);
    CHECK(rule_eq(d.life.rule, rule_brain()));
    CHECK(life_population(&d.life) > 0);
    run(&d, 50);

    director_on_break_end(&d);
    CHECK_INT(d.mood, MOOD_IDLE);
    CHECK(d.sweeping);
    guard = 0;
    while (d.sweeping && guard++ < 500)
        director_frame(&d);
    Life ref;
    life_init(&ref, 96, 48);
    garden_stamp(&g, &ref);
    CHECK(same_board(&d.life, &ref));
    CHECK(rule_eq(d.life.rule, rule_conway()));
    life_free(&ref);
    director_free(&d);
    garden_free(&g);
}

static void test_river_and_wolf(void)
{
    Director d;
    director_init(&d, 120, 64, 9, NULL);
    director_on_wave_start(&d);
    director_on_wave_finish(&d);
    int guard = 0;
    while (!d.sweeping && guard++ < 100)
        director_frame(&d);
    CHECK(d.sweeping);
    int half = director_river_half(&d);
    CHECK(half >= 1);
    int last_x = -1000, seen = 0;
    guard = 0;
    while (d.sweeping && guard++ < 500) {
        for (int x = 0; x < d.life.w; x++) {
            double y = director_river_y(&d, x);
            CHECK(y >= half && y <= d.life.h - 1 - half);
        }
        WolfSpot w;
        if (director_wolf(&d, &w)) {
            CHECK(w.x > last_x); /* always running forward */
            CHECK(w.y >= 0 && w.y + WOLF_H <= d.life.h);
            CHECK(w.frame >= 0 && w.frame < WOLF_FRAMES);
            CHECK_INT(w.kind, WOLF_SNOW); /* the river's own wolf */
            last_x = w.x;
            seen++;
        }
        director_frame(&d);
    }
    CHECK(seen > 10);
    CHECK(!d.sweeping);
    CHECK(rule_eq(d.life.rule, rule_brain()));
    director_free(&d);

    /* no room for the wolf: it skips its run */
    director_init(&d, 30, 10, 9, NULL);
    director_on_wave_start(&d);
    director_on_wave_finish(&d);
    guard = 0;
    while (guard++ < 300) {
        WolfSpot w;
        CHECK(!director_wolf(&d, &w));
        director_frame(&d);
    }
    director_free(&d);
}

/* Frames until a wolf of a kind other than `not` is on the board; -1 if none comes. */
static int until_wolf(Director *d, int not, int limit, WolfSpot *w)
{
    for (int i = 0; i < limit; i++) {
        if (director_wolf(d, w) && w->kind != not)
            return i;
        director_frame(d);
    }
    return -1;
}

static void test_task_wolves(void)
{
    Director d;
    director_init(&d, 120, 64, 11, NULL);
    director_on_task_done(&d);
    CHECK(director_animating(&d));
    WolfSpot w;
    CHECK(until_wolf(&d, WOLF_SNOW, 60, &w) >= 0);
    int first = w.kind, last_x = -1000, seen = 0;
    CHECK(first != WOLF_SNOW); /* the snow wolf belongs to the river */
    for (int guard = 0; guard < 400 && director_wolf(&d, &w); guard++) {
        CHECK_INT(w.kind, first);
        CHECK(w.x > last_x);
        CHECK(w.y >= 0 && w.y + WOLF_H <= d.life.h);
        CHECK(w.frame >= 0 && w.frame < WOLF_FRAMES);
        last_x = w.x;
        seen++;
        director_frame(&d);
    }
    CHECK(seen > 20);
    CHECK(last_x + WOLF_W > d.life.w - 4); /* it ran all the way across */
    run(&d, 300);
    CHECK(!director_wolf(&d, &w));
    CHECK(!director_animating(&d)); /* back to the quiet garden */

    /* three tasks in a row: a different wolf for each, one after another */
    director_on_task_done(&d);
    director_on_task_done(&d);
    director_on_task_done(&d);
    int kinds[3], n = 0, prev = -1;
    for (int guard = 0; guard < 3000 && n < 3; guard++) {
        int k = director_wolf(&d, &w) ? w.kind : -1;
        if (k >= 0 && k != prev)
            kinds[n++] = k;
        prev = k;
        director_frame(&d);
    }
    CHECK_INT(n, 3);
    for (int i = 0; i < n; i++) {
        CHECK(kinds[i] != WOLF_SNOW && kinds[i] != first);
        for (int k = 0; k < i; k++)
            CHECK(kinds[i] != kinds[k]);
    }
    director_free(&d);

    /* no room on the board: no wolf, and nothing keeps animating */
    director_init(&d, 50, 20, 13, NULL);
    director_on_task_done(&d);
    CHECK_INT(until_wolf(&d, -1, 400, &w), -1);
    CHECK(!director_animating(&d));
    director_free(&d);
}

static void test_river_waits_for_task_wolf(void)
{
    Director d;
    director_init(&d, 120, 64, 12, NULL);
    director_on_wave_start(&d);
    director_on_task_done(&d);
    WolfSpot w;
    CHECK(until_wolf(&d, WOLF_SNOW, 60, &w) >= 0);
    director_on_wave_finish(&d);
    /* the river holds back until the task wolf is across */
    int guard = 0;
    while (director_wolf(&d, &w) && w.kind != WOLF_SNOW && guard++ < 500) {
        CHECK(!d.sweeping);
        director_frame(&d);
    }
    CHECK(until_wolf(&d, -1, 500, &w) >= 0);
    CHECK_INT(w.kind, WOLF_SNOW);
    CHECK(d.sweeping);
    /* a task done while the river crosses sends its wolf after it */
    director_on_task_done(&d);
    for (guard = 0; d.sweeping && guard < 1000; guard++) {
        if (director_wolf(&d, &w))
            CHECK_INT(w.kind, WOLF_SNOW);
        director_frame(&d);
    }
    CHECK(!d.sweeping);
    CHECK(until_wolf(&d, WOLF_SNOW, 60, &w) >= 0);
    director_free(&d);
}

static void test_pause_decays(void)
{
    Director d;
    director_init(&d, 96, 48, 5, NULL);
    director_on_wave_start(&d);
    int start = life_population(&d.life);
    director_on_pause(&d);
    CHECK_INT(d.mood, MOOD_PAUSED);
    run(&d, 200);
    CHECK(life_population(&d.life) < start / 2);
    director_on_resume(&d);
    CHECK_INT(d.mood, MOOD_FOCUS);
    CHECK(life_population(&d.life) > 0); /* reseeded when it ran low */
    director_free(&d);
}

static void test_abandon(void)
{
    Garden g;
    small_garden(&g);
    Director d;
    director_init(&d, 96, 48, 6, &g);
    director_on_wave_start(&d);
    director_on_wave_finish(&d);
    director_on_abandon(&d);
    CHECK_INT(d.mood, MOOD_IDLE);
    CHECK(!d.sweeping);
    CHECK_INT(d.pending_sweep, 0);
    director_free(&d);
    garden_free(&g);
}

static void test_tiny_and_resizing_boards(void)
{
    static const int sizes[][2] = {{0, 0}, {1, 1}, {3, 2}, {7, 30}, {200, 3}};
    Garden g;
    small_garden(&g);
    for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++) {
        Director d;
        director_init(&d, sizes[s][0], sizes[s][1], 7, &g);
        director_on_task_done(&d);
        director_on_wave_start(&d);
        director_on_task_done(&d);
        run(&d, 50);
        director_on_pause(&d);
        run(&d, 20);
        director_on_resume(&d);
        director_on_wave_finish(&d);
        run(&d, 300);
        director_resize(&d, sizes[s][1], sizes[s][0]); /* mid-break */
        director_on_break_end(&d);
        run(&d, 300);
        CHECK(d.life.w == sizes[s][1]);
        director_free(&d);
    }
    garden_free(&g);
}

static void test_focus_rule(void)
{
    Director d;
    Rule dn;
    rule_parse("daynight", &dn);
    director_init(&d, 96, 48, 8, NULL);
    CHECK(rule_eq(d.focus_rule, rule_conway()));
    director_set_focus_rule(&d, dn); /* idle: applies to the next wave */
    CHECK(rule_eq(d.life.rule, rule_conway()));
    director_on_wave_start(&d);
    CHECK(rule_eq(d.life.rule, dn));
    run(&d, 900); /* no random HighLife stretches unless the focus rule is Conway */
    CHECK(rule_eq(d.life.rule, dn));

    director_set_focus_rule(&d, rule_conway()); /* during focus: switches live, with a tear */
    CHECK(rule_eq(d.life.rule, rule_conway()));
    CHECK(d.tear > 0);

    director_on_pause(&d);
    director_on_resume(&d);
    CHECK(rule_eq(d.life.rule, rule_conway()));
    director_on_wave_finish(&d);
    int guard = 0;
    while ((d.pending_sweep || d.sweeping) && guard++ < 500)
        director_frame(&d);
    CHECK(rule_eq(d.life.rule, rule_brain())); /* breaks stay Brian's Brain */
    director_free(&d);
}

static void test_deterministic(void)
{
    Director a, b;
    director_init(&a, 80, 40, 1234, NULL);
    director_init(&b, 80, 40, 1234, NULL);
    director_on_wave_start(&a);
    director_on_wave_start(&b);
    director_on_task_done(&a);
    director_on_task_done(&b);
    run(&a, 100);
    run(&b, 100);
    CHECK(same_board(&a.life, &b.life));
    director_free(&a);
    director_free(&b);
}

void suite_director(void)
{
    test_idle_shows_garden();
    test_wave_start_seeds_a_band();
    test_fleet_color();
    test_finish_break_cycle();
    test_river_and_wolf();
    test_task_wolves();
    test_river_waits_for_task_wolf();
    test_pause_decays();
    test_abandon();
    test_tiny_and_resizing_boards();
    test_focus_rule();
    test_deterministic();
}
