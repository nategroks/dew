#include "app.h"

#include "nudge.h"
#include "util.h"

#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static double mono_now(void)
{
    return clock_now(); /* keeps counting through suspend */
}

static void set_str(char **dst, const char *src)
{
    char *copy = src ? xstrdup(src) : NULL;
    free(*dst);
    *dst = copy;
}

static void local_time(const char *fmt, char *out, size_t n)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, fmt, &tm);
}

static void say(App *a, bool warn, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void say(App *a, bool warn, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
    a->msg_until = mono_now() + (warn ? 8 : 4);
    a->msg_warn = warn;
}

/* ---- selection ---- */

static Task *selected(App *a)
{
    return doc_view_at(&a->doc, a->sel, NULL);
}

static void clamp_sel(App *a)
{
    size_t n = doc_view_count(&a->doc);
    if (n == 0)
        a->sel = 0;
    else if (a->sel >= n)
        a->sel = n - 1;
}

static void select_id(App *a, unsigned id)
{
    long i = doc_view_index(&a->doc, id);
    if (i >= 0)
        a->sel = (size_t)i;
    clamp_sel(a);
}

static void move_sel(App *a, int delta)
{
    size_t n = doc_view_count(&a->doc);
    if (n == 0)
        return;
    if (delta < 0 && a->sel > 0)
        a->sel--;
    else if (delta > 0 && a->sel + 1 < n)
        a->sel++;
}

static void refind_active(App *a)
{
    Task *t = a->active_title ? doc_find_title(&a->doc, a->active_title, NULL, NULL) : NULL;
    a->active_id = t ? t->id : 0;
}

/* ---- file sync and saving ---- */

static void sync_file(App *a)
{
    Task *t = selected(a);
    char *keep = t ? xstrdup(t->title) : NULL;
    size_t keep_sel = a->sel;
    ParseError e;
    int r = tasks_reload_if_changed(a->paths.tasks_path, &a->doc, &a->stamp, &e);
    if (r == 1) {
        a->file_broken = false;
        Task *k = keep ? doc_find_title(&a->doc, keep, NULL, NULL) : NULL;
        if (k)
            select_id(a, k->id);
        else {
            a->sel = keep_sel;
            clamp_sel(a);
        }
        refind_active(a);
    } else if (r == -1) {
        if (!a->file_broken)
            say(a, true, "tasks.md line %zu: %s. dew won't change it until it's fixed.", e.line, e.msg);
        a->file_broken = true;
        a->broken_line = e.line;
    }
    free(keep);
}

#define MUT_REFUSED (-100)

/* Lock, reload if changed, and refuse while tasks.md is broken (a change made
   now would be lost when the fixed file is reloaded). */
static int mut_begin(App *a)
{
    int lk = lock_file_begin(a->paths.tasks_path);
    sync_file(a);
    if (a->file_broken) {
        lock_file_end(lk);
        say(a, true, "tasks.md line %zu has an error. Fix it first; dew won't change the list until then.",
            a->broken_line);
        return MUT_REFUSED;
    }
    return lk;
}

static void mut_end(App *a, int lk, bool changed)
{
    if (changed) {
        if (!tasks_save(a->paths.tasks_path, &a->doc, &a->stamp))
            say(a, true, "Could not save %s: %s", a->paths.tasks_path, strerror(errno));
    }
    lock_file_end(lk);
    clamp_sel(a);
}

static void load_garden(App *a)
{
    char *path = garden_file(&a->paths, a->today);
    char *text = slurp(path, NULL);
    if (text) {
        garden_parse(&a->garden, text);
        free(text);
    }
    free(path);
}

static void save_garden(App *a)
{
    char *path = garden_file(&a->paths, a->today);
    char *text = garden_serialize(&a->garden);
    if (!write_atomic(path, text, strlen(text), false))
        say(a, true, "Could not save the garden: %s", strerror(errno));
    free(text);
    free(path);
}

static void save_state(App *a)
{
    SavedState s;
    memset(&s, 0, sizeof s);
    memcpy(s.date, a->today, sizeof s.date);
    WaveSnap w = wave_snapshot(&a->wave, mono_now());
    s.mode = w.mode;
    s.long_break = w.long_break;
    s.remaining = w.remaining;
    s.wall_end = (long long)time(NULL) + (long long)(w.remaining + 0.5);
    rule_format(a->dir.focus_rule, s.rule, sizeof s.rule);
    rule_format(a->cfg.focus_rule, s.base, sizeof s.base);
    s.task = a->active_title; /* borrowed, not freed */
    if (!state_save(a->paths.state_path, &s))
        say(a, true, "Could not save the wave state: %s", strerror(errno));
}

/* ---- task actions ---- */

static void act_toggle(App *a)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    Task *t = selected(a);
    bool now_done = false;
    if (t) {
        task_toggle_done(t, a->today);
        now_done = t->done;
    }
    mut_end(a, lk, t != NULL);
    if (now_done)
        director_on_task_done(&a->dir);
}

static void act_move_list(App *a)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    Task *t = selected(a);
    unsigned id = t ? t->id : 0;
    if (t)
        doc_move_list(&a->doc, id);
    mut_end(a, lk, t != NULL);
    if (t)
        select_id(a, id);
}

static void act_reorder(App *a, int delta)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    Task *t = selected(a);
    unsigned id = t ? t->id : 0;
    bool moved = t && doc_reorder(&a->doc, id, delta);
    mut_end(a, lk, moved);
    if (moved)
        select_id(a, id);
}

/* The task an edit/delete prompt was opened on, even if tasks.md was reloaded since. */
static Task *prompt_target(App *a)
{
    Task *t = doc_find(&a->doc, a->prompt_id, NULL, NULL);
    if (t && a->prompt_title && strcmp(t->title, a->prompt_title) == 0)
        return t;
    return a->prompt_title ? doc_find_title(&a->doc, a->prompt_title, NULL, NULL) : NULL;
}

static void act_delete(App *a)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    Task *t = prompt_target(a);
    if (!t)
        say(a, true, "That task changed on disk, so nothing was deleted.");
    if (t && t->id == a->active_id) {
        a->active_id = 0;
        set_str(&a->active_title, NULL);
    }
    bool gone = t && doc_delete(&a->doc, t->id);
    mut_end(a, lk, gone);
}

static void act_add(App *a, const char *title)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    unsigned id = doc_add(&a->doc, a->add_list, title)->id;
    mut_end(a, lk, true);
    select_id(a, id);
}

static void act_edit(App *a, const char *title)
{
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED)
        return;
    Task *t = prompt_target(a);
    if (!t)
        say(a, true, "That task changed on disk, so nothing was renamed.");
    if (t) {
        bool was_active = t->id == a->active_id;
        task_set_title(t, title);
        if (was_active)
            set_str(&a->active_title, t->title);
    }
    mut_end(a, lk, t != NULL);
}

static char *run_editor(App *a, const char *text)
{
    const char *tmpdir = getenv("TMPDIR");
    char path[512];
    snprintf(path, sizeof path, "%s/dew-note-XXXXXX.md", tmpdir && *tmpdir ? tmpdir : "/tmp");
    int fd = mkstemps(path, 3);
    if (fd < 0) {
        say(a, true, "Could not create a temp file for the note: %s", strerror(errno));
        return NULL;
    }
    size_t len = strlen(text);
    bool ok = write(fd, text, len) == (ssize_t)len && (len == 0 || write(fd, "\n", 1) == 1);
    close(fd);
    if (!ok) {
        unlink(path);
        say(a, true, "Could not write the note to a temp file.");
        return NULL;
    }

    const char *ed = getenv("VISUAL");
    if (!ed || !*ed)
        ed = getenv("EDITOR");
    if (!ed || !*ed)
        ed = "vi";
    setenv("DEW_EDITOR", ed, 1); /* word-split by sh, so "emacsclient -t" works */

    def_prog_mode();
    endwin();
    char *argv[] = {"sh", "-c", "exec $DEW_EDITOR \"$1\"", "sh", path, NULL};
    pid_t pid;
    int status = -1;
    if (posix_spawnp(&pid, "sh", NULL, NULL, argv, environ) == 0)
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    reset_prog_mode();
    refresh();

    char *out = NULL;
    if (status == 0)
        out = slurp(path, NULL);
    else
        say(a, true, "%s exited with an error; the note is unchanged.", ed);
    unlink(path);
    if (out) {
        size_t n = strlen(out);
        while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\t'))
            out[--n] = '\0';
    }
    return out;
}

static void act_note(App *a)
{
    sync_file(a);
    if (a->file_broken) {
        say(a, true, "tasks.md line %zu has an error. Fix it first; dew won't change the list until then.",
            a->broken_line);
        return;
    }
    Task *t = selected(a);
    if (!t)
        return;
    unsigned id = t->id;
    char *title = xstrdup(t->title);
    char *note = run_editor(a, t->note ? t->note : "");
    if (!note) {
        free(title);
        return;
    }
    int lk = mut_begin(a);
    if (lk == MUT_REFUSED) {
        free(note);
        free(title);
        return;
    }
    Task *u = doc_find(&a->doc, id, NULL, NULL);
    if (!u || strcmp(u->title, title) != 0)
        u = doc_find_title(&a->doc, title, NULL, NULL); /* the file was reloaded meanwhile */
    if (u)
        task_set_note(u, note);
    else
        say(a, true, "That task is gone, so the note was not saved.");
    mut_end(a, lk, u != NULL);
    free(note);
    free(title);
}

/* ---- waves ---- */

static void count_wave(App *a)
{
    int lk = mut_begin(a);
    if (lk != MUT_REFUSED) {
        Task *t = a->active_id ? doc_find(&a->doc, a->active_id, NULL, NULL) : NULL;
        if (t)
            t->waves++;
        mut_end(a, lk, t != NULL);
    }

    char hm[6];
    local_time("%H:%M", hm, sizeof hm);
    garden_plant(&a->garden, &a->fx, hm);
    save_garden(a);
    director_on_wave_finish(&a->dir);
    if (a->cfg.bell)
        beep();
    nudge(&a->cfg, a->wave.long_break ? "Wave done. Take a long break." : "Wave done. Take a break.");
    save_state(a);
}

static void tick_wave(App *a, double now)
{
    WaveEvent ev;
    while ((ev = wave_tick(&a->wave, now)) != WEV_NONE) {
        if (ev == WEV_FOCUS_DONE) {
            count_wave(a);
        } else {
            director_on_break_end(&a->dir);
            if (a->cfg.bell)
                beep();
            nudge(&a->cfg, "Break's over.");
            save_state(a);
        }
    }
}

static Task *first_open_today(App *a)
{
    for (size_t i = 0; i < doc_count(&a->doc, LIST_TODAY); i++) {
        Task *t = doc_task_at(&a->doc, LIST_TODAY, i);
        if (!t->done)
            return t;
    }
    return NULL;
}

static void act_space(App *a, double now)
{
    switch (a->wave.mode) {
    case WAVE_IDLE:
    case WAVE_BREAK: {
        Task *t = selected(a);
        if (!t || t->done)
            t = first_open_today(a);
        if (!t) {
            say(a, false, "Nothing to work on. Press a to add a task.");
            return;
        }
        wave_start(&a->wave, now);
        a->active_id = t->id;
        set_str(&a->active_title, t->title);
        director_on_wave_start(&a->dir);
        break;
    }
    case WAVE_FOCUS:
        wave_pause(&a->wave, now);
        director_on_pause(&a->dir);
        break;
    case WAVE_PAUSED:
        wave_resume(&a->wave, now);
        director_on_resume(&a->dir);
        break;
    }
    save_state(a);
}

static void act_stop(App *a, double now)
{
    if (wave_finish(&a->wave, now)) {
        count_wave(a);
    } else if (wave_end_break(&a->wave)) {
        director_on_break_end(&a->dir);
        save_state(a);
    }
}

/* r: the presets in order, plus the config's own rule if it is a custom one. */
static void act_cycle_rule(App *a)
{
    Rule list[16];
    int n = 0;
    for (int i = 0; i < RULE_PRESET_COUNT && n < 15; i++)
        list[n++] = RULE_PRESETS[i].rule;
    if (strcmp(rule_name(a->cfg.focus_rule), "custom") == 0)
        list[n++] = a->cfg.focus_rule;
    int cur = -1;
    for (int i = 0; i < n; i++)
        if (rule_eq(list[i], a->dir.focus_rule))
            cur = i;
    Rule next = list[(cur + 1) % n];
    director_set_focus_rule(&a->dir, next);
    char text[32];
    rule_format(next, text, sizeof text);
    say(a, false, "Waves now run %s (%s).", rule_name(next), text);
    save_state(a);
}

static void act_abandon(App *a)
{
    if (!wave_abandon(&a->wave))
        return;
    director_on_abandon(&a->dir);
    say(a, false, "Wave abandoned. It doesn't count.");
    save_state(a);
}

static void restore_wave(App *a)
{
    SavedState s;
    if (!state_load(a->paths.state_path, &s)) {
        state_clear(&s);
        return;
    }
    /* The rule picked with r is remembered, unless the config's focus_rule changed since. */
    Rule saved, base;
    if (rule_parse(s.rule, &saved) &&
        (!rule_parse(s.base, &base) || rule_eq(base, a->cfg.focus_rule)))
        director_set_focus_rule(&a->dir, saved);
    if (s.mode == WAVE_IDLE) {
        state_clear(&s);
        return;
    }
    double now = mono_now();
    WaveSnap w = {s.mode, s.mode == WAVE_PAUSED ? s.remaining : (double)(s.wall_end - (long long)time(NULL)),
                  s.long_break};
    wave_restore(&a->wave, w, now);
    set_str(&a->active_title, s.task && *s.task ? s.task : NULL);
    state_clear(&s);
    refind_active(a);
    if (s.mode == WAVE_BREAK) {
        director_on_wave_finish(&a->dir);
    } else {
        director_on_wave_start(&a->dir);
        if (s.mode == WAVE_PAUSED)
            director_on_pause(&a->dir);
    }
    tick_wave(a, now);
}

/* ---- day rollover ---- */

static void check_day(App *a)
{
    char today[11];
    local_time("%Y-%m-%d", today, sizeof today);
    if (strcmp(today, a->today) == 0)
        return;
    memcpy(a->today, today, sizeof today);
    int lk = mut_begin(a);
    if (lk != MUT_REFUSED)
        mut_end(a, lk, doc_archive(&a->doc, a->today) > 0);
    garden_free(&a->garden);
    load_garden(a);
    a->wave.waves_today = (int)a->garden.n;
    if (a->dir.mood == MOOD_IDLE && !a->dir.sweeping)
        director_show_garden(&a->dir);
}

/* ---- layout and views ---- */

static void open_garden_view(App *a)
{
    if (a->garden_view)
        life_free(&a->gview);
    life_init(&a->gview, (a->cols - 2) * 2, (a->rows - 3) * 4);
    garden_stamp(&a->garden, &a->gview);
    a->garden_view = true;
}

static void close_garden_view(App *a)
{
    if (!a->garden_view)
        return;
    life_free(&a->gview);
    a->garden_view = false;
}

static void relayout(App *a)
{
    getmaxyx(stdscr, a->rows, a->cols);
    a->lay = layout_compute(a->cols, a->rows);
    int w = 0, h = 0;
    if (a->lay.show_life) {
        w = (a->lay.life.w - 2) * 2;
        h = (a->lay.life.h - 2) * 4;
    }
    director_resize(&a->dir, w, h);
    if (a->garden_view)
        open_garden_view(a);
}

/* ---- keys ---- */

static void start_prompt(App *a, PromptKind kind)
{
    Task *t = selected(a);
    if (kind != PROMPT_ADD && !t)
        return;
    a->prompt = kind;
    if (kind == PROMPT_ADD) {
        ListId l = LIST_TODAY;
        doc_view_at(&a->doc, a->sel, &l);
        a->add_list = t ? l : LIST_TODAY;
        le_init(&a->le, "");
    } else {
        a->prompt_id = t->id;
        set_str(&a->prompt_title, t->title);
        if (kind == PROMPT_EDIT)
            le_init(&a->le, t->title);
    }
}

static void prompt_key(App *a, bool code, wint_t ch)
{
    if (a->prompt == PROMPT_DELETE) {
        if (!code && (ch == 'y' || ch == 'Y'))
            act_delete(a);
        a->prompt = PROMPT_NONE;
        return;
    }
    uint32_t key;
    if (code) {
        switch (ch) {
        case KEY_LEFT: key = LE_KEY_LEFT; break;
        case KEY_RIGHT: key = LE_KEY_RIGHT; break;
        case KEY_HOME: key = LE_KEY_HOME; break;
        case KEY_END: key = LE_KEY_END; break;
        case KEY_BACKSPACE: key = LE_KEY_BACKSPACE; break;
        case KEY_DC: key = LE_KEY_DELETE; break;
        case KEY_ENTER: key = '\n'; break;
        default: return;
        }
    } else {
        key = (uint32_t)ch;
    }
    LeResult r = le_key(&a->le, key);
    if (r == LE_CANCEL) {
        a->prompt = PROMPT_NONE;
        return;
    }
    if (r != LE_COMMIT)
        return;
    char *text = le_text(&a->le);
    bool blank = true;
    for (const char *p = text; *p; p++)
        blank &= *p == ' ';
    if (blank)
        say(a, false, "Empty title; nothing changed.");
    else if (a->prompt == PROMPT_ADD)
        act_add(a, text);
    else
        act_edit(a, text);
    free(text);
    a->prompt = PROMPT_NONE;
}

static void on_key(App *a, bool code, wint_t ch)
{
    if (code && ch == KEY_RESIZE) {
        relayout(a);
        return;
    }
    if (a->help) {
        a->help = false;
        return;
    }
    if (a->garden_view) {
        close_garden_view(a);
        return;
    }
    if (a->prompt != PROMPT_NONE) {
        prompt_key(a, code, ch);
        return;
    }
    size_t n = doc_view_count(&a->doc), nt = doc_count(&a->doc, LIST_TODAY);
    if (code) {
        if (ch == KEY_DOWN)
            move_sel(a, 1);
        else if (ch == KEY_UP)
            move_sel(a, -1);
        else if (ch == KEY_HOME)
            a->sel = 0;
        else if (ch == KEY_END && n)
            a->sel = n - 1;
        return;
    }
    switch (ch) {
    case 'j': move_sel(a, 1); break;
    case 'k': move_sel(a, -1); break;
    case 'g': a->sel = 0; break;
    case 'G': a->sel = n ? n - 1 : 0; break;
    case '\t': a->sel = a->sel < nt && nt < n ? nt : 0; break;
    case 'a': start_prompt(a, PROMPT_ADD); break;
    case 'e': start_prompt(a, PROMPT_EDIT); break;
    case 'd': start_prompt(a, PROMPT_DELETE); break;
    case 'n': act_note(a); break;
    case 'x': act_toggle(a); break;
    case 't': act_move_list(a); break;
    case 'J': act_reorder(a, 1); break;
    case 'K': act_reorder(a, -1); break;
    case ' ': act_space(a, a->now); break;
    case 's': act_stop(a, a->now); break;
    case 'S': act_abandon(a); break;
    case 'r': act_cycle_rule(a); break;
    case 'w': open_garden_view(a); break;
    case '?': a->help = true; break;
    case 'q': a->quit = true; break;
    default: break;
    }
}

/* ---- setup and the main loop ---- */

static int setup(App *a, const char *file)
{
    config_defaults(&a->cfg);
    a->instance_fd = -1;
    if (!paths_init(&a->paths, file)) {
        fprintf(stderr, "dew: cannot create %s: %s\n",
                a->paths.data_dir ? a->paths.data_dir : "the data directory", strerror(errno));
        return 1;
    }
    Sbuf warn;
    sb_init(&warn);
    char *text = slurp(a->paths.config_path, NULL);
    if (text) {
        config_parse(&a->cfg, text, &warn);
        free(text);
    }
    text = slurp(a->paths.colors_path, NULL);
    if (text) {
        config_parse_colors(&a->cfg, text, &warn);
        free(text);
    }
    if (warn.len) {
        fputs(warn.buf, stderr);
        char *nl = strchr(warn.buf, '\n');
        if (nl)
            *nl = '\0';
        say(a, true, "%s", warn.buf);
    }
    sb_free(&warn);
    palette_compute(&a->rgb, a->cfg.nord);

    a->instance_fd = lock_instance(a->paths.lock_path);
    if (a->instance_fd == -1) {
        fputs("dew: already running in another terminal (dew add still works)\n", stderr);
        return 1;
    }
    if (a->instance_fd < 0) {
        fprintf(stderr, "dew: cannot open %s: %s\n", a->paths.lock_path, strerror(errno));
        return 1;
    }

    local_time("%Y-%m-%d", a->today, sizeof a->today);
    int lk = lock_file_begin(a->paths.tasks_path);
    ParseError e;
    LoadResult r = tasks_load(a->paths.tasks_path, &a->doc, &e, &a->stamp);
    if (r == LOAD_PARSE_ERROR) {
        lock_file_end(lk);
        fprintf(stderr,
                "dew: %s:%zu: %s\n"
                "Fix that line and run dew again. dew never overwrites a file it can't read.\n",
                a->paths.tasks_path, e.line, e.msg);
        return 1;
    }
    if (r == LOAD_IO_ERROR) {
        lock_file_end(lk);
        fprintf(stderr, "dew: cannot read %s: %s\n", a->paths.tasks_path, strerror(errno));
        return 1;
    }
    if (doc_archive(&a->doc, a->today) > 0 && !tasks_save(a->paths.tasks_path, &a->doc, &a->stamp))
        say(a, true, "Could not save %s: %s", a->paths.tasks_path, strerror(errno));
    lock_file_end(lk);

    garden_init(&a->garden);
    load_garden(a);
    WaveCfg wc = {a->cfg.focus_min * 60, a->cfg.short_min * 60, a->cfg.long_min * 60,
                  a->cfg.long_every};
    wave_init(&a->wave, wc);
    a->wave.waves_today = (int)a->garden.n;
    rng_seed(&a->fx, (uint64_t)time(NULL) ^ ((uint64_t)getpid() << 20));
    uint64_t seed = rng_next(&a->fx) | (uint64_t)rng_next(&a->fx) << 32;
    director_init(&a->dir, 0, 0, seed, &a->garden);
    director_set_focus_rule(&a->dir, a->cfg.focus_rule);
    return 0;
}

static void teardown(App *a)
{
    close_garden_view(a);
    director_free(&a->dir);
    garden_free(&a->garden);
    if (a->doc.secs)
        doc_free(&a->doc);
    if (a->instance_fd >= 0)
        lock_file_end(a->instance_fd);
    free(a->active_title);
    free(a->prompt_title);
    paths_free(&a->paths);
}

int app_main(const char *tasks_override)
{
    App *a = xcalloc(1, sizeof *a);
    if (setup(a, tasks_override) != 0) {
        teardown(a);
        free(a);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    render_init_colors();
    relayout(a);
    restore_wave(a);

    const double frame = 1.0 / a->cfg.fps;
    a->next_frame = mono_now();
    while (!a->quit && !g_stop) {
        a->now = mono_now();
        tick_wave(a, a->now);
        if (a->now >= a->next_check) {
            a->next_check = a->now + 1;
            check_day(a);
            sync_file(a);
            nudge_reap();
        }
        if (a->now >= a->next_frame) {
            int steps = director_animating(&a->dir) ? 1 : 8;
            for (int i = 0; i < steps; i++) {
                director_frame(&a->dir);
                if (a->garden_view && a->dir.frame % 8 == 0)
                    life_step(&a->gview);
            }
            a->next_frame = a->now + frame * steps;
        }
        render(a);

        double wait = a->next_frame - mono_now();
        if (wait > 0.25)
            wait = 0.25; /* keep the clock in the status line ticking */
        timeout(wait > 0 ? (int)(wait * 1000) : 0);
        wint_t ch;
        int r = get_wch(&ch);
        a->now = mono_now();
        if (r == KEY_CODE_YES)
            on_key(a, true, ch);
        else if (r == OK)
            on_key(a, false, ch);
    }

    save_state(a);
    endwin();
    render_restore_colors();
    teardown(a);
    free(a);
    return 0;
}
