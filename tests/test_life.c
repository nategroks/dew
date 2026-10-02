#include "life.h"
#include "patterns.h"
#include "test.h"

static int alive(const Life *l, int x, int y)
{
    return life_get(l, x, y) == CELL_ON;
}

static void test_blinker(void)
{
    Life l;
    life_init(&l, 20, 20);
    for (int x = 5; x <= 7; x++)
        life_set(&l, x, 5, CELL_ON, 2, 0);
    life_step(&l);
    CHECK(alive(&l, 6, 4) && alive(&l, 6, 5) && alive(&l, 6, 6));
    CHECK(!alive(&l, 5, 5) && !alive(&l, 7, 5));
    CHECK_INT(life_tint(&l, 6, 4), 2); /* newborns inherit the neighbors' tint */
    CHECK_INT(life_age(&l, 6, 5), 1);  /* the center survived */
    CHECK_INT(life_age(&l, 6, 4), 0);
    life_step(&l);
    CHECK(alive(&l, 5, 5) && alive(&l, 6, 5) && alive(&l, 7, 5));
    CHECK_INT(life_population(&l), 3);
    life_free(&l);
}

static void test_block_is_stable(void)
{
    Life l;
    life_init(&l, 10, 10);
    Pattern p;
    CHECK(pattern_get("block", &p));
    pattern_stamp(&l, &p, 3, 3, 0, 0);
    for (int i = 0; i < 5; i++)
        life_step(&l);
    CHECK_INT(life_population(&l), 4);
    CHECK_INT(life_age(&l, 3, 3), 5);
    life_free(&l);
}

static void test_glider_moves(void)
{
    Life l;
    life_init(&l, 30, 30);
    Pattern p;
    CHECK(pattern_get("glider", &p));
    pattern_stamp(&l, &p, 2, 2, 1, 0);
    for (int i = 0; i < 4; i++)
        life_step(&l);
    CHECK_INT(life_population(&l), 5);
    for (int i = 0; i < p.n; i++)
        CHECK(alive(&l, 3 + p.xy[2 * i], 3 + p.xy[2 * i + 1]));
    life_free(&l);
}

static void test_highlife_b6(void)
{
    /* the center cell has exactly 6 live neighbors */
    static const int nb[6][2] = {{4, 4}, {5, 4}, {6, 4}, {4, 6}, {5, 6}, {6, 6}};
    for (int r = 0; r < 2; r++) {
        Life l;
        life_init(&l, 12, 12);
        life_set_rule(&l, r ? rule_highlife() : rule_conway());
        for (int k = 0; k < 6; k++)
            life_set(&l, nb[k][0], nb[k][1], CELL_ON, 0, 0);
        life_step(&l);
        CHECK_INT(alive(&l, 5, 5), r == 1);
        life_free(&l);
    }
}

static void test_brain(void)
{
    Life l;
    life_init(&l, 12, 12);
    life_set_rule(&l, rule_brain());
    life_set(&l, 4, 5, CELL_ON, 3, 0);
    life_set(&l, 6, 5, CELL_ON, 3, 0);
    life_step(&l);
    CHECK_INT(life_get(&l, 4, 5), CELL_DYING);
    CHECK_INT(life_get(&l, 5, 4), CELL_ON); /* two on-neighbors: born */
    CHECK_INT(life_tint(&l, 5, 4), 3);
    life_step(&l);
    CHECK_INT(life_get(&l, 4, 5), CELL_OFF); /* dying -> off */

    life_set(&l, 1, 1, CELL_DYING, 0, 0);
    life_set_rule(&l, rule_conway());
    CHECK_INT(life_get(&l, 1, 1), CELL_OFF);
    life_free(&l);
}

static void test_generic_rules(void)
{
    Rule r;
    Life l;

    /* Seeds B2/S: live cells always die, a dead cell with two neighbours is born */
    life_init(&l, 12, 12);
    rule_parse("seeds", &r);
    life_set_rule(&l, r);
    life_set(&l, 4, 5, CELL_ON, 0, 0);
    life_set(&l, 6, 5, CELL_ON, 0, 0);
    life_step(&l);
    CHECK_INT(life_get(&l, 4, 5), CELL_OFF);
    CHECK_INT(life_get(&l, 5, 4), CELL_ON);
    CHECK_INT(life_get(&l, 5, 6), CELL_ON);
    life_free(&l);

    /* Star Wars B2/S345/4: a lonely cell fades through states 2 and 3, then turns off */
    life_init(&l, 12, 12);
    rule_parse("starwars", &r);
    life_set_rule(&l, r);
    life_set(&l, 5, 5, CELL_ON, 0, 0);
    life_step(&l);
    CHECK_INT(life_get(&l, 5, 5), 2);
    life_step(&l);
    CHECK_INT(life_get(&l, 5, 5), 3);
    life_step(&l);
    CHECK_INT(life_get(&l, 5, 5), CELL_OFF);
    CHECK_INT(life_population(&l), 0);
    life_free(&l);

    /* Life without Death B3/S012345678: nothing ever dies */
    life_init(&l, 12, 12);
    rule_parse("lifewithoutdeath", &r);
    life_set_rule(&l, r);
    life_set(&l, 5, 5, CELL_ON, 0, 0);
    for (int i = 0; i < 5; i++)
        life_step(&l);
    CHECK_INT(life_get(&l, 5, 5), CELL_ON);
    life_free(&l);

    /* switching from a 4-state rule to a 3-state one drops cells in state 3 */
    life_init(&l, 12, 12);
    rule_parse("starwars", &r);
    life_set_rule(&l, r);
    life_set(&l, 2, 2, 3, 0, 0);
    life_set(&l, 3, 3, 2, 0, 0);
    life_set_rule(&l, rule_brain());
    CHECK_INT(life_get(&l, 2, 2), CELL_OFF);
    CHECK_INT(life_get(&l, 3, 3), 2);
    life_free(&l);
}

static void test_margin_absorbs(void)
{
    Life l;
    life_init(&l, 10, 10);
    Pattern p;
    pattern_get("glider", &p);
    pattern_stamp(&l, &p, 4, 4, 0, 0);
    for (int i = 0; i < 120; i++)
        life_step(&l);
    CHECK_INT(life_population(&l), 0);
    life_free(&l);
}

static void test_resize_and_tiny(void)
{
    Life l;
    life_init(&l, 8, 8);
    life_set(&l, 1, 1, CELL_ON, 4, 7);
    life_set(&l, 7, 7, CELL_ON, 0, 0);
    life_resize(&l, 4, 4);
    CHECK_INT(life_get(&l, 1, 1), CELL_ON);
    CHECK_INT(life_tint(&l, 1, 1), 4);
    CHECK_INT(life_age(&l, 1, 1), 7);
    CHECK_INT(life_population(&l), 1);

    life_resize(&l, 0, 0);
    life_set(&l, 0, 0, CELL_ON, 0, 0); /* inside the margin; allowed */
    life_set(&l, 500, 500, CELL_ON, 0, 0);
    life_step(&l);
    CHECK_INT(life_population(&l), 0);
    life_free(&l);

    life_init(&l, -5, 3);
    CHECK_INT(l.w, 0);
    life_step(&l);
    life_free(&l);
}

static void test_patterns(void)
{
    Pattern p;
    CHECK(pattern_get("glider", &p));
    CHECK_INT(p.n, 5);
    CHECK_INT(p.w, 3);
    CHECK_INT(p.h, 3);
    pattern_flip(&p, true, false);
    CHECK_INT(p.xy[0], 1); /* ".O." stays centered */
    CHECK_INT(p.xy[2], 0); /* "..O" -> "O.." */
    CHECK(pattern_get("pulsar", &p));
    CHECK_INT(p.n, 48);
    CHECK_INT(p.w, 13);
    CHECK_INT(p.h, 13);
    CHECK(pattern_get("lwss", &p));
    CHECK_INT(p.n, 9);
    CHECK(pattern_get("penta", &p));
    CHECK_INT(p.n, 12);
    CHECK(!pattern_get("spaceship-of-theseus", &p));
    CHECK_INT(pattern_period("block"), 1);
    CHECK_INT(pattern_period("blinker"), 2);
    CHECK_INT(pattern_period("pulsar"), 3);
    CHECK_INT(pattern_period("penta"), 15);
}

void suite_life(void)
{
    test_blinker();
    test_block_is_stable();
    test_glider_moves();
    test_highlife_b6();
    test_brain();
    test_generic_rules();
    test_margin_absorbs();
    test_resize_and_tiny();
    test_patterns();
}
