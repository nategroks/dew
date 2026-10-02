#include "taskfile.h"
#include "test.h"

#include <stdlib.h>

static const char SAMPLE[] =
    "<!-- my dew list -->\n"
    "\n"
    "# Today\n"
    "- [ ] fix grub theme on the ESP <!-- dew waves=2 -->\n"
    "  GRUB lives on /boot (noauto ESP).\n"
    "  \n"
    "    indented code line\n"
    "- [x] polymon P0 scaffold <!-- dew waves=3 done=2026-10-02 -->\n"
    "some stray text\n"
    "\n"
    "# Backlog\n"
    "- [ ] retheme sddm login\n"
    "- [ ] 🌊 surf <!-- dew waves=1 color=blue -->\n"
    "\n"
    "# Notes\n"
    "- [ ] not a real task here\n"
    "\n"
    "# Done 2026-10-01\n"
    "- [x] tv-underscan helper <!-- dew waves=1 done=2026-10-01 -->\n";

static bool parse(const char *text, TaskDoc *d, ParseError *e)
{
    return taskfile_parse(text, strlen(text), d, e);
}

static void test_round_trip(void)
{
    TaskDoc d;
    ParseError e;
    CHECK(parse(SAMPLE, &d, &e));
    size_t len;
    char *out = taskfile_serialize(&d, &len);
    CHECK_STR(out, SAMPLE);
    CHECK_INT(len, strlen(SAMPLE));
    free(out);
    doc_free(&d);
}

static void test_fields(void)
{
    TaskDoc d;
    ParseError e;
    CHECK(parse(SAMPLE, &d, &e));
    CHECK_INT(doc_count(&d, LIST_TODAY), 2);
    CHECK_INT(doc_count(&d, LIST_BACKLOG), 2);

    Task *t = doc_task_at(&d, LIST_TODAY, 0);
    CHECK_STR(t->title, "fix grub theme on the ESP");
    CHECK_INT(t->waves, 2);
    CHECK(!t->done);
    CHECK_STR(t->note, "GRUB lives on /boot (noauto ESP).\n\n  indented code line");

    t = doc_task_at(&d, LIST_TODAY, 1);
    CHECK(t->done);
    CHECK_STR(t->done_date, "2026-10-02");
    CHECK(t->note == NULL);

    t = doc_task_at(&d, LIST_BACKLOG, 1);
    CHECK_STR(t->title, "🌊 surf");
    CHECK_STR(t->extra_meta, "color=blue");
    doc_free(&d);
}

static void test_ambiguous_titles(void)
{
    TaskDoc d;
    ParseError e;
    /* not a closed dew comment: the whole thing is the title */
    CHECK(parse("# Today\n- [ ] see <!-- dew foo\n- [ ] <!-- dewy -->\n", &d, &e));
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 0)->title, "see <!-- dew foo");
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 1)->title, "<!-- dewy -->");
    doc_free(&d);

    /* a title that itself ends in a dew-looking comment survives a round trip */
    TaskDoc a;
    doc_init(&a);
    doc_add(&a, LIST_TODAY, "x <!-- dew waves=9 -->");
    char *text = taskfile_serialize(&a, NULL);
    CHECK(parse(text, &d, &e));
    Task *t = doc_task_at(&d, LIST_TODAY, 0);
    CHECK_STR(t->title, "x <!-- dew waves=9 -->");
    CHECK_INT(t->waves, 0);
    free(text);
    doc_free(&a);
    doc_free(&d);
}

static void test_errors(void)
{
    TaskDoc d;
    ParseError e;
    CHECK(!parse("# Today\n- [ ] a\n- [ ] b <!-- dew waves=abc -->\n", &d, &e));
    CHECK_INT(e.line, 3);
    CHECK(strstr(e.msg, "waves") != NULL);

    CHECK(!parse("# Today\n- [x] a <!-- dew done=2026-13-40 -->\n", &d, &e));
    CHECK_INT(e.line, 2);

    CHECK(!parse("# Today\n- [ ] a <!-- dew waves=-1 -->\n", &d, &e));
    CHECK_INT(e.line, 2);

    const char nul[] = "# Today\n- [ ] a\0b\n";
    CHECK(!taskfile_parse(nul, sizeof nul - 1, &d, &e));
    CHECK_INT(e.line, 2);
}

static void test_edges(void)
{
    TaskDoc d;
    ParseError e;
    char *out;
    /* no trailing newline; the serializer adds one */
    CHECK(parse("# Today\n- [ ] a", &d, &e));
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 0)->title, "a");
    doc_free(&d);

    /* a second "# Today" is kept but its tasks are not shown */
    CHECK(parse("# Today\n- [ ] a\n# Today\n- [ ] b\n", &d, &e));
    CHECK_INT(doc_count(&d, LIST_TODAY), 1);
    out = taskfile_serialize(&d, NULL);
    CHECK_STR(out, "# Today\n- [ ] a\n# Today\n- [ ] b\n");
    free(out);
    doc_free(&d);

    /* an indented line right after a heading is raw, not a note */
    CHECK(parse("# Today\n  hello\n- [ ] a\n", &d, &e));
    CHECK(doc_task_at(&d, LIST_TODAY, 0)->note == NULL);
    doc_free(&d);

    /* Windows line endings */
    CHECK(parse("# Today\r\n- [ ] a <!-- dew waves=2 -->\r\n  note\r\n", &d, &e));
    CHECK_INT(doc_count(&d, LIST_TODAY), 1);
    CHECK_INT(doc_task_at(&d, LIST_TODAY, 0)->waves, 2);
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 0)->note, "note");
    out = taskfile_serialize(&d, NULL);
    CHECK_STR(out, "# Today\n- [ ] a <!-- dew waves=2 -->\n  note\n");
    free(out);
    doc_free(&d);

    /* empty input, then the default skeleton */
    CHECK(parse("", &d, &e));
    doc_ensure_lists(&d);
    out = taskfile_serialize(&d, NULL);
    CHECK_STR(out, "# Today\n\n# Backlog\n");
    free(out);
    doc_free(&d);
}

static void test_add_into_skeleton(void)
{
    TaskDoc d;
    ParseError e;
    CHECK(parse("# Today\n\n# Backlog\n", &d, &e));
    doc_add(&d, LIST_TODAY, "first");
    doc_add(&d, LIST_BACKLOG, "later");
    Task *t = doc_add(&d, LIST_TODAY, "second");
    task_set_note(t, "two\nlines");
    char *out = taskfile_serialize(&d, NULL);
    CHECK_STR(out, "# Today\n- [ ] first\n- [ ] second\n  two\n  lines\n\n# Backlog\n- [ ] later\n");
    free(out);
    doc_free(&d);
}

void suite_taskfile(void)
{
    test_round_trip();
    test_fields();
    test_ambiguous_titles();
    test_errors();
    test_edges();
    test_add_into_skeleton();
}
