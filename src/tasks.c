#include "tasks.h"

#include "runes.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *clean_title(const char *title)
{
    char *t = xstrdup(title);
    for (char *p = t; *p; p++)
        if ((unsigned char)*p < 0x20 || *p == 0x7f)
            *p = ' ';
    return t;
}

static void set_date(char dst[11], const char *src)
{
    size_t n = strlen(src);
    if (n > 10)
        n = 10;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void task_free(Task *t)
{
    free(t->title);
    free(t->note);
    free(t->extra_meta);
}

static void item_free(Item *it)
{
    if (it->kind == ITEM_TASK)
        task_free(&it->task);
    else
        free(it->raw);
}

void doc_init(TaskDoc *d)
{
    d->secs = NULL;
    d->n = d->cap = 0;
    d->next_id = 1;
    doc_add_section(d, SEC_PREAMBLE, NULL);
}

void doc_free(TaskDoc *d)
{
    for (size_t i = 0; i < d->n; i++) {
        Section *s = &d->secs[i];
        for (size_t j = 0; j < s->n; j++)
            item_free(&s->items[j]);
        free(s->items);
        free(s->heading);
    }
    free(d->secs);
    d->secs = NULL;
    d->n = d->cap = 0;
}

Section *doc_add_section(TaskDoc *d, SecKind kind, const char *heading)
{
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 4;
        d->secs = xrealloc(d->secs, d->cap * sizeof *d->secs);
    }
    Section *s = &d->secs[d->n++];
    s->kind = kind;
    s->heading = heading ? xstrdup(heading) : NULL;
    s->items = NULL;
    s->n = s->cap = 0;
    return s;
}

static Item *section_insert(Section *s, size_t at)
{
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->items = xrealloc(s->items, s->cap * sizeof *s->items);
    }
    memmove(&s->items[at + 1], &s->items[at], (s->n - at) * sizeof *s->items);
    s->n++;
    Item *it = &s->items[at];
    memset(it, 0, sizeof *it);
    return it;
}

static void section_remove(Section *s, size_t pos, bool free_item)
{
    if (free_item)
        item_free(&s->items[pos]);
    memmove(&s->items[pos], &s->items[pos + 1], (s->n - pos - 1) * sizeof *s->items);
    s->n--;
}

void section_push_raw(Section *s, const char *line)
{
    Item *it = section_insert(s, s->n);
    it->kind = ITEM_RAW;
    it->raw = xstrdup(line);
}

static void task_new(TaskDoc *d, Task *t, const char *title)
{
    memset(t, 0, sizeof *t);
    t->id = d->next_id++;
    t->title = clean_title(title);
    t->rune = RUNE_NONE;
}

Task *section_push_task(TaskDoc *d, Section *s, const char *title)
{
    Item *it = section_insert(s, s->n);
    it->kind = ITEM_TASK;
    task_new(d, &it->task, title);
    return &it->task;
}

static Section *list_section(const TaskDoc *d, ListId l)
{
    SecKind k = l == LIST_TODAY ? SEC_TODAY : SEC_BACKLOG;
    for (size_t i = 0; i < d->n; i++)
        if (d->secs[i].kind == k)
            return &d->secs[i];
    return NULL;
}

static bool is_blank(const char *s)
{
    for (; *s; s++)
        if (*s != ' ' && *s != '\t')
            return false;
    return true;
}

static bool ends_blank(const Section *s)
{
    return s->n > 0 && s->items[s->n - 1].kind == ITEM_RAW && is_blank(s->items[s->n - 1].raw);
}

/* New tasks go after the last non-blank item, so trailing blank lines stay trailing. */
static size_t insert_pos(const Section *s)
{
    size_t at = s->n;
    while (at > 0 && s->items[at - 1].kind == ITEM_RAW && is_blank(s->items[at - 1].raw))
        at--;
    return at;
}

/* Keeps one blank line between the previous section and a new one. */
static Section *append_section(TaskDoc *d, SecKind kind, const char *heading)
{
    Section *last = &d->secs[d->n - 1];
    if ((last->heading || last->n > 0) && !ends_blank(last))
        section_push_raw(last, "");
    return doc_add_section(d, kind, heading);
}

void doc_ensure_lists(TaskDoc *d)
{
    if (!list_section(d, LIST_TODAY))
        append_section(d, SEC_TODAY, "# Today");
    if (!list_section(d, LIST_BACKLOG))
        append_section(d, SEC_BACKLOG, "# Backlog");
}

size_t doc_count(const TaskDoc *d, ListId l)
{
    const Section *s = list_section(d, l);
    size_t n = 0;
    if (s)
        for (size_t i = 0; i < s->n; i++)
            if (s->items[i].kind == ITEM_TASK)
                n++;
    return n;
}

Task *doc_task_at(TaskDoc *d, ListId l, size_t idx)
{
    Section *s = list_section(d, l);
    if (!s)
        return NULL;
    size_t k = 0;
    for (size_t i = 0; i < s->n; i++) {
        if (s->items[i].kind != ITEM_TASK)
            continue;
        if (k == idx)
            return &s->items[i].task;
        k++;
    }
    return NULL;
}

size_t doc_view_count(const TaskDoc *d)
{
    return doc_count(d, LIST_TODAY) + doc_count(d, LIST_BACKLOG);
}

Task *doc_view_at(TaskDoc *d, size_t i, ListId *list)
{
    size_t nt = doc_count(d, LIST_TODAY);
    if (i < nt) {
        if (list)
            *list = LIST_TODAY;
        return doc_task_at(d, LIST_TODAY, i);
    }
    if (list)
        *list = LIST_BACKLOG;
    return doc_task_at(d, LIST_BACKLOG, i - nt);
}

static bool locate(const TaskDoc *d, unsigned id, const char *title, ListId *list, Section **sec,
                   size_t *pos, size_t *idx)
{
    for (int li = 0; li < 2; li++) {
        Section *s = list_section(d, (ListId)li);
        if (!s)
            continue;
        size_t k = 0;
        for (size_t i = 0; i < s->n; i++) {
            if (s->items[i].kind != ITEM_TASK)
                continue;
            const Task *t = &s->items[i].task;
            if (title ? strcmp(t->title, title) == 0 : t->id == id) {
                if (list)
                    *list = (ListId)li;
                if (sec)
                    *sec = s;
                if (pos)
                    *pos = i;
                if (idx)
                    *idx = k;
                return true;
            }
            k++;
        }
    }
    return false;
}

long doc_view_index(TaskDoc *d, unsigned id)
{
    ListId l;
    size_t idx;
    if (!locate(d, id, NULL, &l, NULL, NULL, &idx))
        return -1;
    return l == LIST_TODAY ? (long)idx : (long)(doc_count(d, LIST_TODAY) + idx);
}

Task *doc_find(TaskDoc *d, unsigned id, ListId *list, size_t *idx)
{
    Section *s;
    size_t pos;
    if (!locate(d, id, NULL, list, &s, &pos, idx))
        return NULL;
    return &s->items[pos].task;
}

Task *doc_find_title(TaskDoc *d, const char *title, ListId *list, size_t *idx)
{
    Section *s;
    size_t pos;
    if (!locate(d, 0, title, list, &s, &pos, idx))
        return NULL;
    return &s->items[pos].task;
}

Task *doc_add(TaskDoc *d, ListId l, const char *title)
{
    doc_ensure_lists(d);
    Section *s = list_section(d, l);
    Item *it = section_insert(s, insert_pos(s));
    it->kind = ITEM_TASK;
    task_new(d, &it->task, title);
    return &it->task;
}

void task_set_title(Task *t, const char *title)
{
    free(t->title);
    t->title = clean_title(title);
}

void task_set_note(Task *t, const char *note)
{
    free(t->note);
    t->note = note && *note ? xstrdup(note) : NULL;
}

void task_toggle_done(Task *t, const char *today)
{
    t->done = !t->done;
    if (t->done)
        set_date(t->done_date, today);
    else
        t->done_date[0] = '\0';
}

void doc_next_rune(TaskDoc *d, Task *t)
{
    bool taken[RUNE_ART_COUNT] = {false};
    for (size_t i = 0; i < doc_view_count(d); i++) {
        const Task *o = doc_view_at(d, i, NULL);
        if (o != t && o->rune >= 0 && o->rune < RUNE_ART_COUNT)
            taken[o->rune] = true;
    }
    int from = t->rune < 0 ? RUNE_ART_COUNT - 1 : t->rune;
    for (int k = 1; k < RUNE_ART_COUNT; k++)
        if (!taken[(from + k) % RUNE_ART_COUNT]) {
            t->rune = (from + k) % RUNE_ART_COUNT;
            return;
        }
    t->rune = (from + 1) % RUNE_ART_COUNT;
}

int doc_assign_runes(TaskDoc *d, unsigned start)
{
    int used[RUNE_ART_COUNT] = {0}, given = 0;
    size_t n = doc_view_count(d);
    for (size_t i = 0; i < n; i++) {
        int r = doc_view_at(d, i, NULL)->rune;
        if (r >= 0 && r < RUNE_ART_COUNT)
            used[r]++;
    }
    for (size_t i = 0; i < n; i++) {
        Task *t = doc_view_at(d, i, NULL);
        if (t->rune >= 0 && t->rune < RUNE_ART_COUNT)
            continue;
        int best = -1;
        for (int k = 0; k < RUNE_ART_COUNT; k++) {
            int r = (int)((start + (unsigned)k) % RUNE_ART_COUNT);
            if (best < 0 || used[r] < used[best])
                best = r;
        }
        t->rune = best;
        used[best]++;
        start = (unsigned)best + 1; /* the next one picks on from here */
        given++;
    }
    return given;
}

bool doc_delete(TaskDoc *d, unsigned id)
{
    Section *s;
    size_t pos;
    if (!locate(d, id, NULL, NULL, &s, &pos, NULL))
        return false;
    section_remove(s, pos, true);
    return true;
}

bool doc_move_list(TaskDoc *d, unsigned id)
{
    Section *s;
    size_t pos;
    ListId l;
    if (!locate(d, id, NULL, &l, &s, &pos, NULL))
        return false;
    Item copy = s->items[pos];
    section_remove(s, pos, false);
    Section *dst = list_section(d, l == LIST_TODAY ? LIST_BACKLOG : LIST_TODAY);
    *section_insert(dst, insert_pos(dst)) = copy;
    return true;
}

bool doc_reorder(TaskDoc *d, unsigned id, int delta)
{
    Section *s;
    size_t pos;
    if (!locate(d, id, NULL, NULL, &s, &pos, NULL) || delta == 0)
        return false;
    long j = (long)pos;
    do
        j += delta > 0 ? 1 : -1;
    while (j >= 0 && j < (long)s->n && s->items[j].kind != ITEM_TASK);
    if (j < 0 || j >= (long)s->n)
        return false;
    Item tmp = s->items[pos];
    s->items[pos] = s->items[j];
    s->items[j] = tmp;
    return true;
}

static Section *done_section(TaskDoc *d, const char *date)
{
    char heading[32];
    snprintf(heading, sizeof heading, "# Done %.10s", date);
    for (size_t i = 0; i < d->n; i++)
        if (d->secs[i].kind == SEC_DONE && strcmp(d->secs[i].heading, heading) == 0)
            return &d->secs[i];
    return append_section(d, SEC_DONE, heading);
}

int doc_archive(TaskDoc *d, const char *today)
{
    int moved = 0;
    Section *s = list_section(d, LIST_TODAY);
    if (!s)
        return 0;
    for (size_t i = 0; i < s->n;) {
        Item *it = &s->items[i];
        if (it->kind != ITEM_TASK || !it->task.done) {
            i++;
            continue;
        }
        if (!it->task.done_date[0])
            set_date(it->task.done_date, today);
        if (strcmp(it->task.done_date, today) >= 0) {
            i++;
            continue;
        }
        Item copy = *it;
        section_remove(s, i, false);
        Section *ds = done_section(d, copy.task.done_date); /* may move d->secs */
        *section_insert(ds, insert_pos(ds)) = copy;
        s = list_section(d, LIST_TODAY);
        moved++;
    }
    return moved;
}
