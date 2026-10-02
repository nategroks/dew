#include "director.h"
#include "test.h"

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
    test_pause_decays();
    test_abandon();
    test_tiny_and_resizing_boards();
    test_focus_rule();
    test_deterministic();
}
