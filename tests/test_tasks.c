#include "tasks.h"
#include "test.h"

static void fresh(TaskDoc *d)
{
    doc_init(d);
    doc_ensure_lists(d);
}

static const Section *find_heading(const TaskDoc *d, const char *heading)
{
    for (size_t i = 0; i < d->n; i++)
        if (d->secs[i].heading && strcmp(d->secs[i].heading, heading) == 0)
            return &d->secs[i];
    return NULL;
}

static void test_add_and_view(void)
{
    TaskDoc d;
    fresh(&d);
    CHECK_INT(doc_view_count(&d), 0);
    CHECK(doc_view_at(&d, 0, NULL) == NULL);

    doc_add(&d, LIST_BACKLOG, "retheme sddm");
    doc_add(&d, LIST_TODAY, "fix grub");
    doc_add(&d, LIST_TODAY, "email site");
    CHECK_INT(doc_count(&d, LIST_TODAY), 2);
    CHECK_INT(doc_count(&d, LIST_BACKLOG), 1);

    ListId l;
    CHECK_STR(doc_view_at(&d, 0, &l)->title, "fix grub");
    CHECK_INT(l, LIST_TODAY);
    CHECK_STR(doc_view_at(&d, 1, NULL)->title, "email site");
    CHECK_STR(doc_view_at(&d, 2, &l)->title, "retheme sddm");
    CHECK_INT(l, LIST_BACKLOG);
    CHECK(doc_view_at(&d, 3, NULL) == NULL);

    unsigned id = doc_view_at(&d, 2, NULL)->id;
    CHECK_INT(doc_view_index(&d, id), 2);
    CHECK_INT(doc_view_index(&d, 9999), -1);
    doc_free(&d);
}

static void test_title_is_cleaned(void)
{
    TaskDoc d;
    fresh(&d);
    Task *t = doc_add(&d, LIST_TODAY, "two\nlines\tand\x7f" "bell\a");
    CHECK_STR(t->title, "two lines and bell ");
    task_set_title(t, "a\rb");
    CHECK_STR(t->title, "a b");
    doc_free(&d);
}

static void test_toggle_done(void)
{
    TaskDoc d;
    fresh(&d);
    Task *t = doc_add(&d, LIST_TODAY, "x");
    task_toggle_done(t, "2026-10-02");
    CHECK(t->done);
    CHECK_STR(t->done_date, "2026-10-02");
    task_toggle_done(t, "2026-10-02");
    CHECK(!t->done);
    CHECK_STR(t->done_date, "");
    doc_free(&d);
}

static void test_note(void)
{
    TaskDoc d;
    fresh(&d);
    Task *t = doc_add(&d, LIST_TODAY, "x");
    task_set_note(t, "line one\nline two");
    CHECK_STR(t->note, "line one\nline two");
    task_set_note(t, "");
    CHECK(t->note == NULL);
    doc_free(&d);
}

static void test_reorder(void)
{
    TaskDoc d;
    fresh(&d);
    unsigned a = doc_add(&d, LIST_TODAY, "a")->id;
    doc_add(&d, LIST_TODAY, "b");
    unsigned c = doc_add(&d, LIST_TODAY, "c")->id;

    CHECK(!doc_reorder(&d, a, -1)); /* already first */
    CHECK(doc_reorder(&d, a, +1));
    CHECK_STR(doc_view_at(&d, 0, NULL)->title, "b");
    CHECK_STR(doc_view_at(&d, 1, NULL)->title, "a");
    CHECK(!doc_reorder(&d, c, +1)); /* already last */
    CHECK(doc_reorder(&d, c, -1));
    CHECK_STR(doc_view_at(&d, 1, NULL)->title, "c");
    CHECK(!doc_reorder(&d, 4242, 1));
    doc_free(&d);
}

static void test_move_and_delete(void)
{
    TaskDoc d;
    fresh(&d);
    unsigned a = doc_add(&d, LIST_TODAY, "a")->id;
    doc_add(&d, LIST_BACKLOG, "b");
    CHECK(doc_move_list(&d, a));
    CHECK_INT(doc_count(&d, LIST_TODAY), 0);
    CHECK_STR(doc_task_at(&d, LIST_BACKLOG, 1)->title, "a"); /* goes to the end */
    CHECK(doc_move_list(&d, a));
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 0)->title, "a");

    CHECK(doc_delete(&d, a));
    CHECK(!doc_delete(&d, a));
    CHECK_INT(doc_view_count(&d), 1);
    doc_free(&d);
}

static void test_find_title(void)
{
    TaskDoc d;
    fresh(&d);
    doc_add(&d, LIST_TODAY, "a");
    doc_add(&d, LIST_BACKLOG, "b");
    ListId l;
    size_t idx;
    Task *t = doc_find_title(&d, "b", &l, &idx);
    CHECK(t != NULL);
    CHECK_INT(l, LIST_BACKLOG);
    CHECK_INT(idx, 0);
    CHECK(doc_find_title(&d, "zzz", NULL, NULL) == NULL);
    doc_free(&d);
}

static void test_archive(void)
{
    TaskDoc d;
    fresh(&d);
    Task *old = doc_add(&d, LIST_TODAY, "yesterday's");
    task_toggle_done(old, "2026-10-01");
    Task *now = doc_add(&d, LIST_TODAY, "today's");
    task_toggle_done(now, "2026-10-02");
    Task *undated = doc_add(&d, LIST_TODAY, "undated");
    undated->done = true;
    doc_add(&d, LIST_TODAY, "open");

    CHECK_INT(doc_archive(&d, "2026-10-02"), 1);
    CHECK_INT(doc_count(&d, LIST_TODAY), 3);
    const Section *done = find_heading(&d, "# Done 2026-10-01");
    CHECK(done != NULL);
    if (done) {
        CHECK_INT(done->kind, SEC_DONE);
        CHECK(done->n >= 1 && done->items[0].kind == ITEM_TASK);
        CHECK_STR(done->items[0].task.title, "yesterday's");
    }
    CHECK_STR(doc_find_title(&d, "undated", NULL, NULL)->done_date, "2026-10-02");

    /* the next day both of today's done tasks join a new section */
    CHECK_INT(doc_archive(&d, "2026-10-03"), 2);
    done = find_heading(&d, "# Done 2026-10-02");
    CHECK(done != NULL && done->n >= 2);
    CHECK_INT(doc_count(&d, LIST_TODAY), 1);

    /* archiving again into an existing section appends */
    Task *more = doc_add(&d, LIST_TODAY, "late");
    task_toggle_done(more, "2026-10-01");
    CHECK_INT(doc_archive(&d, "2026-10-03"), 1);
    done = find_heading(&d, "# Done 2026-10-01");
    size_t tasks = 0;
    for (size_t i = 0; done && i < done->n; i++)
        tasks += done->items[i].kind == ITEM_TASK;
    CHECK_INT(tasks, 2);
    doc_free(&d);
}

static void test_ensure_lists_idempotent(void)
{
    TaskDoc d;
    fresh(&d);
    size_t n = d.n;
    doc_ensure_lists(&d);
    CHECK_INT(d.n, n);
    CHECK_INT(d.n, 3); /* preamble, Today, Backlog */
    doc_free(&d);
}

void suite_tasks(void)
{
    test_add_and_view();
    test_title_is_cleaned();
    test_toggle_done();
    test_note();
    test_reorder();
    test_move_and_delete();
    test_find_title();
    test_archive();
    test_ensure_lists_idempotent();
}
