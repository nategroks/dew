#include "config.h"
#include "runes.h"
#include "taskfile.h"
#include "test.h"

#include <stdlib.h>

static void test_lookup(void)
{
    CHECK_INT(rune_find("fehu"), 0);
    CHECK_INT(rune_find("othala"), RUNE_ART_COUNT - 1);
    CHECK_INT(rune_find("Algiz"), rune_find("algiz")); /* any case */
    CHECK_INT(rune_find("perth"), rune_find("perthro")); /* the chart's spellings */
    CHECK_INT(rune_find("TEIWAZ"), rune_find("tiwaz"));
    CHECK_INT(rune_find("inguz"), rune_find("ingwaz"));
    CHECK_INT(rune_find("bogus"), RUNE_NONE);
    CHECK_INT(rune_find(""), RUNE_NONE);
    CHECK_STR(rune_name(rune_find("kano")), "kenaz");
    CHECK_STR(rune_name(RUNE_NONE), "");
    CHECK_STR(rune_name(99), "");
    for (int i = 0; i < RUNE_ART_COUNT; i++) {
        CHECK_INT(rune_find(rune_name(i)), i);
        CHECK(RUNE_ART[i].cp >= 0x16A0 && RUNE_ART[i].cp <= 0x16F8);
        CHECK(RUNE_ART[i].meaning[0]);
    }
    CHECK_INT(RUNE_ART[rune_find("laguz")].cp, 0x16DA); /* the wave counter's rune */
}

static void test_art_and_color(void)
{
    Config c;
    config_defaults(&c);
    for (int i = 0; i < RUNE_ART_COUNT; i++) {
        int ink = 0;
        for (int y = 0; y < RUNE_ART_H; y++)
            for (int x = 0; x < RUNE_ART_W; x++)
                ink += rune_pixel(i, x, y);
        CHECK(ink > 40); /* a real glyph, not a speck */
        CHECK(ink < RUNE_ART_W * RUNE_ART_H / 2);
        if (i > 0)
            CHECK(rune_rgb(c.nord, i) != rune_rgb(c.nord, i - 1)); /* neighbors differ */
    }
    CHECK_INT(rune_pixel(0, -1, 0), 0);
    CHECK_INT(rune_pixel(0, RUNE_ART_W, 0), 0);
    CHECK_INT(rune_pixel(RUNE_NONE, 5, 5), 0);
    /* fehu: yellow, softened toward snow (tools/runes.py computes the same) */
    CHECK_INT(rune_rgb(c.nord, 0), 0xEBD4A5);

    /* a canvas: whole-number scale, centered, only the rune's color, transparent around */
    int w = 60, h = 80;
    uint8_t *px = calloc((size_t)(w * h), 4);
    CHECK_INT(rune_canvas(c.nord, 3, w, h, px), 3);
    int opaque = 0;
    uint32_t want = rune_rgb(c.nord, 3);
    for (int k = 0; k < w * h; k++)
        if (px[4 * k + 3]) {
            opaque++;
            CHECK_INT((uint32_t)px[4 * k] << 16 | (uint32_t)px[4 * k + 1] << 8 | px[4 * k + 2], want);
        }
    int ink = 0;
    for (int y = 0; y < RUNE_ART_H; y++)
        for (int x = 0; x < RUNE_ART_W; x++)
            ink += rune_pixel(3, x, y);
    CHECK_INT(opaque, ink * 9);
    CHECK_INT(px[3], 0);
    CHECK_INT(rune_canvas(c.nord, 3, 5, 5, px), 1); /* too small: cropped, no overrun */
    free(px);
}

static void test_task_runes(void)
{
    TaskDoc d;
    ParseError e;
    const char *text = "# Today\n- [ ] a <!-- dew waves=2 rune=Perth -->\n- [ ] b\n\n# Backlog\n"
                       "- [ ] c <!-- dew rune=algiz color=blue -->\n\n# Done 2026-10-01\n- [x] old\n";
    CHECK(taskfile_parse(text, strlen(text), &d, &e));
    CHECK_INT(doc_task_at(&d, LIST_TODAY, 0)->rune, rune_find("perthro"));
    CHECK_INT(doc_task_at(&d, LIST_TODAY, 1)->rune, RUNE_NONE);
    CHECK_INT(doc_task_at(&d, LIST_BACKLOG, 0)->rune, rune_find("algiz"));
    CHECK_STR(doc_task_at(&d, LIST_BACKLOG, 0)->extra_meta, "color=blue");
    char *out = taskfile_serialize(&d, NULL);
    CHECK(strstr(out, "- [ ] a <!-- dew waves=2 rune=perthro -->\n") != NULL); /* canonical name */
    CHECK(strstr(out, "- [ ] b\n") != NULL);
    CHECK(strstr(out, "- [ ] c <!-- dew rune=algiz color=blue -->\n") != NULL);
    free(out);

    /* the runeless get one each; the done archive is left alone */
    CHECK_INT(doc_assign_runes(&d, 0), 1);
    Task *b = doc_task_at(&d, LIST_TODAY, 1);
    CHECK(b->rune != RUNE_NONE && b->rune != rune_find("perthro") && b->rune != rune_find("algiz"));
    CHECK_INT(doc_assign_runes(&d, 0), 0);
    out = taskfile_serialize(&d, NULL);
    CHECK(strstr(out, "- [x] old\n") != NULL);
    free(out);
    doc_free(&d);

    const char *bad = "# Today\n- [ ] a\n- [ ] b <!-- dew rune=bogus -->\n";
    CHECK(!taskfile_parse(bad, strlen(bad), &d, &e));
    CHECK_INT(e.line, 3);
    CHECK(strstr(e.msg, "rune") != NULL);
}

static void test_spread(void)
{
    TaskDoc d;
    doc_init(&d);
    doc_ensure_lists(&d);
    for (int i = 0; i < 30; i++)
        doc_add(&d, i % 3 ? LIST_TODAY : LIST_BACKLOG, "task");
    CHECK_INT(doc_assign_runes(&d, 7), 30);
    int used[RUNE_ART_COUNT] = {0};
    for (size_t i = 0; i < doc_view_count(&d); i++) {
        int r = doc_view_at(&d, i, NULL)->rune;
        CHECK(r >= 0 && r < RUNE_ART_COUNT);
        if (r >= 0 && r < RUNE_ART_COUNT)
            used[r]++;
    }
    for (int r = 0; r < RUNE_ART_COUNT; r++)
        CHECK(used[r] == 1 || used[r] == 2); /* every rune before any repeats */

    /* a new task takes a rune nobody has, when there is one */
    TaskDoc s;
    doc_init(&s);
    doc_ensure_lists(&s);
    Task *t = doc_add(&s, LIST_TODAY, "one");
    doc_assign_runes(&s, 3);
    int first = t->rune;
    t = doc_add(&s, LIST_TODAY, "two");
    doc_assign_runes(&s, 3);
    CHECK(t->rune != first);
    /* the start shifts which free rune is taken */
    TaskDoc u;
    doc_init(&u);
    doc_ensure_lists(&u);
    doc_add(&u, LIST_TODAY, "one");
    doc_assign_runes(&u, 4);
    CHECK(doc_task_at(&u, LIST_TODAY, 0)->rune != first);

    /* R: the next rune in futhark order that no other task has */
    Task *one = doc_task_at(&s, LIST_TODAY, 0);
    one->rune = 5;
    t->rune = 4;
    doc_next_rune(&s, t);
    CHECK_INT(t->rune, 6); /* 5 is taken */
    t->rune = RUNE_ART_COUNT - 1;
    one->rune = 0;
    doc_next_rune(&s, t);
    CHECK_INT(t->rune, 1); /* wraps, skipping 0 */
    for (int i = 0; i < RUNE_ART_COUNT; i++) /* every rune taken: plain next */
        doc_add(&d, LIST_TODAY, "more");
    Task *last = doc_task_at(&d, LIST_TODAY, 0);
    doc_assign_runes(&d, 0);
    int before = last->rune;
    doc_next_rune(&d, last);
    CHECK_INT(last->rune, (before + 1) % RUNE_ART_COUNT);
    doc_free(&d);
    doc_free(&s);
    doc_free(&u);
}

void suite_runes(void)
{
    test_lookup();
    test_art_and_color();
    test_task_runes();
    test_spread();
}
