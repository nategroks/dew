#include "braille.h"
#include "config.h"
#include "garden.h"
#include "layout.h"
#include "lineedit.h"
#include "palette.h"
#include "sprite.h"
#include "test.h"

#include <stdlib.h>

static void test_palette(void)
{
    Config c;
    config_defaults(&c);
    PaletteRGB p;
    palette_compute(&p, c.nord);
    const uint32_t *n = c.nord;
    CHECK_INT(p.life[0][0], 0xeceff4); /* newborn: snow */
    CHECK_INT(p.life[0][1], rgb_mix(n[13], n[6], 0.2)); /* then soft pastels: yellow, */
    CHECK_INT(p.life[0][2], rgb_mix(n[14], n[6], 0.2)); /* green, */
    CHECK_INT(p.life[0][3], n[7]);                      /* the frosts, */
    CHECK_INT(p.life[0][6], rgb_mix(n[15], n[6], 0.2)); /* mauve */
    CHECK_INT(p.life[0][PAL_AGES - 1], rgb_mix(n[15], n[2], 0.55)); /* old: dusk */
    for (int b = 0; b < PAL_AGES; b++)
        for (int k = 0; k < b; k++)
            CHECK(p.life[0][b] != p.life[0][k]); /* every age its own color */
    CHECK_INT(p.life[1][0], rgb_mix(n[11], n[6], 0.3)); /* fleets: pastel when newborn, */
    CHECK_INT(p.life[1][4], 0xbf616a); /* the five aurora colors... */
    CHECK_INT(p.life[4][4], 0xa3be8c);
    CHECK_INT(p.life[9][4], 0x5e81ac); /* ...then the four frost colors */
    for (int t = 1; t < LIFE_TINTS; t++)
        for (int b = 2; b < PAL_AGES; b += 2)
            CHECK(p.life[t][b] != p.life[t][b - 2]);
    /* list stripes: between the background and the selection, unlike either */
    CHECK(p.stripe != p.bg && p.stripe != p.sel_bg && p.stripe != p.band);
    CHECK(((p.stripe >> 16) & 0xff) > ((p.bg >> 16) & 0xff));
    CHECK(((p.stripe >> 16) & 0xff) < ((p.sel_bg >> 16) & 0xff));
    /* braille wolves: the snow wolf as before, the others in their coats */
    for (int z = 0; z < WOLF_ZONES; z++) {
        CHECK_INT(p.wolf[WOLF_SNOW][z][PX_SNOW - 1], 0xeceff4);
        CHECK_INT(p.wolf[WOLF_SNOW][z][PX_SHADE - 1], 0xd8dee9);
        CHECK_INT(p.wolf[WOLF_SNOW][z][PX_EYE - 1], 0xbf616a);
        CHECK_INT(p.wolf[WOLF_SNOW][z][PX_DARK - 1], 0x4c566a);
    }
    for (int k = 1; k < WOLF_KINDS; k++)
        CHECK(p.wolf[k][1][PX_SNOW - 1] != p.wolf[WOLF_SNOW][1][PX_SNOW - 1]);
    CHECK(p.wolf[WOLF_AURORA][0][PX_SNOW - 1] != p.wolf[WOLF_AURORA][2][PX_SNOW - 1]);
    CHECK_INT(p.garden[0][0], 0xd08770); /* garden pair 0: orange -> purple */
    CHECK_INT(p.garden[0][7], 0xb48ead);
    for (int g = 0; g < GARDEN_PAIRS; g++)
        CHECK(p.garden[g][0] != p.garden[g][7]);
    CHECK_INT(p.bg, 0x2e3440);
    for (int t = 0; t < LIFE_TINTS; t++)
        for (int b = 0; b < PAL_AGES; b++) {
            uint32_t g = p.gray[t][b];
            CHECK(((g >> 16) & 0xff) <= ((g >> 8) & 0xff) && ((g >> 8) & 0xff) <= (g & 0xff));
        }
    CHECK_INT(palette_age_bucket(0), 0);
    CHECK_INT(palette_age_bucket(3), 2);
    CHECK_INT(palette_age_bucket(59), 6);
    CHECK_INT(palette_age_bucket(60000), 7);
    CHECK_INT(rgb_mix(0x000000, 0xffffff, 0.5), 0x808080);
}

/* Every distinct color must fit the 240 redefinable terminal slots, with room to spare. */
static void test_color_budget(void)
{
    Config c;
    config_defaults(&c);
    PaletteRGB p;
    palette_compute(&p, c.nord);
    const uint32_t *all = (const uint32_t *)&p;
    size_t n = sizeof p / sizeof(uint32_t), distinct = 0;
    for (size_t i = 0; i < n; i++) {
        bool seen = false;
        for (size_t k = 0; k < i && !seen; k++)
            seen = all[k] == all[i];
        distinct += !seen;
    }
    CHECK(distinct <= 220);
}

static void test_nearest(void)
{
    CHECK_INT(palette_nearest256(0x000000), 16);
    CHECK_INT(palette_nearest256(0xffffff), 231);
    CHECK_INT(palette_nearest256(0xff0000), 196);
    CHECK_INT(palette_nearest256(0x808080), 244);
    int nord8 = palette_nearest256(0x88c0d0);
    CHECK(nord8 >= 16 && nord8 <= 255);
    CHECK_INT(palette_nearest8(0x000000), 0);
    CHECK_INT(palette_nearest8(0xff0000), 1);
    CHECK_INT(palette_nearest8(0x00ffff), 6);
    CHECK_INT(palette_nearest8(0xeceff4), 7);
}

static void test_braille(void)
{
    Life l;
    life_init(&l, 6, 8);
    BrailleCell c = braille_cell(&l, 0, 0);
    CHECK_INT(c.bits, 0);
    CHECK(!c.dying);
    life_set(&l, 0, 0, CELL_ON, 2, 9);
    life_set(&l, 1, 3, CELL_ON, 4, 1);
    c = braille_cell(&l, 0, 0);
    CHECK_INT(c.bits, 0x01 | 0x80);
    CHECK_INT(c.tint, 4); /* the youngest dot decides */
    CHECK_INT(c.age, 1);
    for (int y = 4; y < 8; y++)
        for (int x = 2; x < 4; x++)
            life_set(&l, x, y, CELL_ON, 0, 0);
    CHECK_INT(braille_cell(&l, 1, 1).bits, 0xff);
    life_set(&l, 4, 0, CELL_DYING, 3, 0);
    c = braille_cell(&l, 2, 0);
    CHECK(c.dying);
    CHECK_INT(c.bits, 0x01);
    CHECK_INT(braille_cell(&l, 9, 9).bits, 0); /* past the board */
    life_free(&l);
}

static void test_layout(void)
{
    Layout l = layout_compute(80, 24);
    CHECK(!l.too_small);
    CHECK(l.show_life);
    CHECK_INT(l.list.w, 32);
    CHECK_INT(l.life.x, 32);
    CHECK_INT(l.life.w, 48);
    CHECK_INT(l.list.h, 20);
    CHECK_INT(l.note.y, 20);
    CHECK_INT(l.keys.y, 23);
    l = layout_compute(79, 24);
    CHECK(!l.show_life);
    CHECK_INT(l.list.w, 79);
    l = layout_compute(80, 23);
    CHECK(!l.show_life);
    CHECK(layout_compute(29, 24).too_small);
    CHECK(layout_compute(80, 7).too_small);
}

static void typed(LineEdit *e, const char *s)
{
    for (; *s; s++)
        le_key(e, (unsigned char)*s);
}

static void check_text(const LineEdit *e, const char *want)
{
    char *t = le_text(e);
    CHECK_STR(t, want);
    free(t);
}

static void test_lineedit(void)
{
    LineEdit e;
    le_init(&e, "héllo 🌊");
    CHECK_INT(e.len, 7);
    CHECK_INT(e.cur, 7);
    check_text(&e, "héllo 🌊");

    le_key(&e, LE_KEY_HOME);
    typed(&e, ">");
    check_text(&e, ">héllo 🌊");
    char *before = le_before_cursor(&e);
    CHECK_STR(before, ">");
    free(before);

    le_key(&e, LE_KEY_END);
    le_key(&e, LE_KEY_BACKSPACE);
    le_key(&e, LE_KEY_LEFT);
    le_key(&e, LE_KEY_DELETE);
    check_text(&e, ">héllo");
    le_key(&e, '\t'); /* control characters are ignored */
    check_text(&e, ">héllo");

    le_key(&e, LE_KEY_LEFT);
    le_key(&e, LE_KEY_LEFT);
    le_key(&e, 21); /* ^U */
    check_text(&e, "lo");
    CHECK_INT(e.cur, 0);
    le_key(&e, LE_KEY_BACKSPACE); /* at the start: nothing */
    check_text(&e, "lo");

    CHECK_INT(le_key(&e, '\n'), LE_COMMIT);
    CHECK_INT(le_key(&e, 27), LE_CANCEL);

    le_init(&e, "");
    for (int i = 0; i < LE_MAX + 10; i++)
        le_key(&e, 'x');
    CHECK_INT(e.len, LE_MAX);
    le_init(&e, "a\tb\x01");
    check_text(&e, "ab");
}

void suite_view(void)
{
    test_palette();
    test_color_budget();
    test_nearest();
    test_braille();
    test_layout();
    test_lineedit();
}
