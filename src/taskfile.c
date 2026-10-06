#include "taskfile.h"

#include "runes.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define META_OPEN "<!-- dew"
#define META_CLOSE "-->"

static bool fail(ParseError *err, size_t line, const char *msg)
{
    err->line = line;
    snprintf(err->msg, sizeof err->msg, "%s", msg);
    return false;
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && memcmp(s + n - m, suffix, m) == 0;
}

static bool valid_date(const char *s)
{
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-')
        return false;
    for (int i = 0; i < 10; i++)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9'))
            return false;
    int month = (s[5] - '0') * 10 + (s[6] - '0');
    int day = (s[8] - '0') * 10 + (s[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

/* The last "<!-- dew" in body that starts a well-formed trailing comment, or NULL. */
static const char *find_meta(const char *body)
{
    if (!ends_with(body, META_CLOSE))
        return NULL;
    const char *m = NULL;
    for (const char *p = strstr(body, META_OPEN); p; p = strstr(p + 1, META_OPEN))
        m = p;
    if (!m)
        return NULL;
    const char *inner = m + strlen(META_OPEN);
    const char *close = body + strlen(body) - strlen(META_CLOSE);
    if (close < inner)
        return NULL;
    if (inner < close && *inner != ' ')
        return NULL; /* e.g. "<!-- dewy -->" is not ours */
    return m;
}

/* Returns 1 for a task line, 0 for not a task, -1 for a malformed dew comment. */
static int parse_task_line(TaskDoc *d, Section *s, const char *line, size_t lineno,
                           ParseError *err)
{
    bool done;
    if (strncmp(line, "- [ ] ", 6) == 0)
        done = false;
    else if (strncmp(line, "- [x] ", 6) == 0 || strncmp(line, "- [X] ", 6) == 0)
        done = true;
    else
        return 0;

    const char *body = line + 6;
    const char *meta = find_meta(body);
    size_t title_len = meta ? (size_t)(meta - body) : strlen(body);
    if (meta && title_len > 0 && body[title_len - 1] == ' ')
        title_len--;

    char *title = xstrndup(body, title_len);
    Task *t = section_push_task(d, s, title);
    free(title);
    t->done = done;
    if (!meta)
        return 1;

    const char *inner = meta + strlen(META_OPEN);
    size_t inner_len = strlen(inner) - strlen(META_CLOSE);
    char *tokens = xstrndup(inner, inner_len);
    Sbuf extra;
    sb_init(&extra);
    int rc = 1;
    for (char *save = NULL, *tok = strtok_r(tokens, " ", &save); tok;
         tok = strtok_r(NULL, " ", &save)) {
        if (strncmp(tok, "waves=", 6) == 0) {
            int v;
            if (!parse_int(tok + 6, &v) || v < 0) {
                fail(err, lineno, "waves must be a whole number >= 0");
                rc = -1;
                break;
            }
            t->waves = v;
        } else if (strncmp(tok, "rune=", 5) == 0) {
            t->rune = rune_find(tok + 5);
            if (t->rune == RUNE_NONE) {
                fail(err, lineno, "rune must be an Elder Futhark rune like fehu, algiz or perthro");
                rc = -1;
                break;
            }
        } else if (strncmp(tok, "done=", 5) == 0) {
            if (!valid_date(tok + 5)) {
                fail(err, lineno, "done must be a date like 2026-10-02");
                rc = -1;
                break;
            }
            memcpy(t->done_date, tok + 5, 11);
        } else {
            if (extra.len)
                sb_puts(&extra, " ");
            sb_puts(&extra, tok);
        }
    }
    if (rc == 1 && extra.len)
        t->extra_meta = sb_take(&extra);
    sb_free(&extra);
    free(tokens);
    return rc;
}

static SecKind heading_kind(const TaskDoc *d, const char *line)
{
    bool have_today = false, have_backlog = false;
    for (size_t i = 0; i < d->n; i++) {
        have_today |= d->secs[i].kind == SEC_TODAY;
        have_backlog |= d->secs[i].kind == SEC_BACKLOG;
    }
    if (strcmp(line, "# Today") == 0 && !have_today)
        return SEC_TODAY;
    if (strcmp(line, "# Backlog") == 0 && !have_backlog)
        return SEC_BACKLOG;
    if (strncmp(line, "# Done ", 7) == 0)
        return SEC_DONE;
    return SEC_OTHER;
}

static void append_note(Task *t, const char *text)
{
    if (!t->note) {
        t->note = xstrdup(text);
        return;
    }
    size_t a = strlen(t->note), b = strlen(text);
    t->note = xrealloc(t->note, a + b + 2);
    t->note[a] = '\n';
    memcpy(t->note + a + 1, text, b + 1);
}

bool taskfile_parse(const char *text, size_t len, TaskDoc *d, ParseError *err)
{
    doc_init(d);
    const char *nul = memchr(text, '\0', len);
    if (nul) {
        size_t line = 1;
        for (const char *p = text; p < nul; p++)
            line += *p == '\n';
        doc_free(d);
        return fail(err, line, "the file contains a NUL byte");
    }

    Section *cur = &d->secs[0];
    bool in_task = false; /* the previous line was a task or one of its note lines */
    size_t lineno = 0;
    const char *p = text, *end = text + len;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t n = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (n > 0 && p[n - 1] == '\r')
            n--; /* CRLF files load fine and are saved back with LF */
        char *line = xstrndup(p, n);
        p = nl ? nl + 1 : end;
        lineno++;

        if (strncmp(line, "# ", 2) == 0) {
            cur = doc_add_section(d, heading_kind(d, line), line);
            in_task = false;
        } else if (cur->kind == SEC_TODAY || cur->kind == SEC_BACKLOG) {
            if (in_task && strncmp(line, "  ", 2) == 0) {
                append_note(&cur->items[cur->n - 1].task, line + 2);
            } else {
                int rc = parse_task_line(d, cur, line, lineno, err);
                if (rc < 0) {
                    free(line);
                    doc_free(d);
                    return false;
                }
                if (rc == 0)
                    section_push_raw(cur, line);
                in_task = rc == 1;
            }
        } else {
            section_push_raw(cur, line);
        }
        free(line);
    }
    return true;
}

static void put_task(Sbuf *b, const Task *t)
{
    sb_puts(b, t->done ? "- [x] " : "- [ ] ");
    sb_puts(b, t->title);
    /* A title ending in "-->" gets an empty dew comment so it can't be misread as one. */
    bool rune = t->rune >= 0 && t->rune < RUNE_ART_COUNT;
    if (t->waves > 0 || t->done_date[0] || rune || t->extra_meta || ends_with(t->title, META_CLOSE)) {
        sb_puts(b, " " META_OPEN);
        if (t->waves > 0)
            sb_printf(b, " waves=%d", t->waves);
        if (t->done_date[0])
            sb_printf(b, " done=%s", t->done_date);
        if (rune)
            sb_printf(b, " rune=%s", rune_name(t->rune));
        if (t->extra_meta)
            sb_printf(b, " %s", t->extra_meta);
        sb_puts(b, " " META_CLOSE);
    }
    sb_puts(b, "\n");
    if (t->note) {
        const char *s = t->note;
        for (;;) {
            const char *nl = strchr(s, '\n');
            sb_puts(b, "  ");
            sb_putn(b, s, nl ? (size_t)(nl - s) : strlen(s));
            sb_puts(b, "\n");
            if (!nl)
                break;
            s = nl + 1;
        }
    }
}

char *taskfile_serialize(const TaskDoc *d, size_t *len_out)
{
    Sbuf b;
    sb_init(&b);
    for (size_t i = 0; i < d->n; i++) {
        const Section *s = &d->secs[i];
        if (s->heading) {
            sb_puts(&b, s->heading);
            sb_puts(&b, "\n");
        }
        for (size_t j = 0; j < s->n; j++) {
            if (s->items[j].kind == ITEM_TASK) {
                put_task(&b, &s->items[j].task);
            } else {
                sb_puts(&b, s->items[j].raw);
                sb_puts(&b, "\n");
            }
        }
    }
    if (len_out)
        *len_out = b.len;
    return sb_take(&b);
}
