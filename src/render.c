#include "app.h"
#include "braille.h"
#include "sprite.h"
#include "wolfview.h"
#include "util.h"

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---- colors: exact Nord where the terminal lets us redefine slots ---- */

#define MAX_COLORS 240
#define MAX_PAIRS 1024

static struct {
    uint32_t rgb;
    short idx;
} colors[MAX_COLORS];
static struct {
    short fg, bg, pair;
} pairs[MAX_PAIRS];
static int ncolors, npairs, depth; /* depth: 0 none, 8, 256 */
static bool redefine, changed;

void render_init_colors(void)
{
    ncolors = npairs = 0;
    changed = false;
    depth = 0;
    if (!has_colors())
        return;
    start_color();
    depth = COLORS >= 256 ? 256 : 8;
    redefine = depth == 256 && can_change_color();
}

void render_restore_colors(void)
{
    if (changed) {
        fputs("\033]104\007", stdout); /* OSC 104: reset the whole palette */
        fflush(stdout);
    }
}

static short color_index(uint32_t rgb)
{
    for (int i = 0; i < ncolors; i++)
        if (colors[i].rgb == rgb)
            return colors[i].idx;
    short idx;
    if (redefine && ncolors < MAX_COLORS) {
        idx = (short)(16 + ncolors);
        init_color(idx, (short)(((rgb >> 16) & 0xff) * 1000 / 255),
                   (short)(((rgb >> 8) & 0xff) * 1000 / 255), (short)((rgb & 0xff) * 1000 / 255));
        changed = true;
    } else {
        idx = (short)(depth == 256 ? palette_nearest256(rgb) : palette_nearest8(rgb));
    }
    if (ncolors < MAX_COLORS) {
        colors[ncolors].rgb = rgb;
        colors[ncolors].idx = idx;
        ncolors++;
    }
    return idx;
}

static short pair_of(uint32_t fg, uint32_t bg)
{
    if (depth == 0)
        return 0;
    short f = color_index(fg), b = color_index(bg);
    if (depth == 8 && f == b) /* e.g. nord3 borders on nord0 both map to black */
        f = b == COLOR_BLACK ? COLOR_BLUE : COLOR_BLACK;
    for (int i = 0; i < npairs; i++)
        if (pairs[i].fg == f && pairs[i].bg == b)
            return pairs[i].pair;
    if (npairs >= MAX_PAIRS || npairs + 1 >= COLOR_PAIRS)
        return 0;
    short p = (short)(npairs + 1);
    init_pair(p, f, b);
    pairs[npairs].fg = f;
    pairs[npairs].bg = b;
    pairs[npairs].pair = p;
    npairs++;
    return p;
}

static void pen(uint32_t fg, uint32_t bg, attr_t attrs)
{
    attr_set(attrs, pair_of(fg, bg), NULL);
}

/* ---- text ---- */

static int cp_width(uint32_t cp)
{
    int w = wcwidth((wchar_t)cp);
    return w < 0 ? 1 : w;
}

static int text_width(const char *s)
{
    int w = 0;
    uint32_t cp;
    int n;
    while ((n = utf8_decode(s, &cp)) > 0) {
        w += cp_width(cp);
        s += n;
    }
    return w;
}

/* Draws s at (y, x), skipping its first `skip` columns, in at most maxw columns.
   Ends with "…" when it doesn't fit. Returns the columns used. */
static int put_text(int y, int x, int maxw, const char *s, int skip)
{
    if (maxw <= 0)
        return 0;
    bool cut = text_width(s) - skip > maxw;
    int room = cut ? maxw - 1 : maxw, used = 0, col = 0;
    move(y, x);
    uint32_t cp;
    int n;
    while ((n = utf8_decode(s, &cp)) > 0) {
        int w = cp_width(cp);
        s += n;
        if (col < skip) {
            col += w;
            continue;
        }
        if (used + w > room)
            break;
        char buf[4];
        addnstr(buf, utf8_encode(cp, buf));
        used += w;
    }
    if (cut) {
        addstr("…");
        used++;
    }
    return used;
}

static void fill(int y, int x, int n)
{
    move(y, x);
    for (int i = 0; i < n; i++)
        addch(' ');
}

static void draw_box(App *a, Rect r, const char *left, const char *right, const char *bottom)
{
    const PaletteRGB *p = &a->rgb;
    if (r.w < 2 || r.h < 2)
        return;
    pen(p->ui[UI_BORDER], p->bg, A_NORMAL);
    mvaddstr(r.y, r.x, "┌");
    mvaddstr(r.y + r.h - 1, r.x, "└");
    for (int x = 1; x < r.w - 1; x++) {
        mvaddstr(r.y, r.x + x, "─");
        mvaddstr(r.y + r.h - 1, r.x + x, "─");
    }
    mvaddstr(r.y, r.x + r.w - 1, "┐");
    mvaddstr(r.y + r.h - 1, r.x + r.w - 1, "┘");
    for (int y = 1; y < r.h - 1; y++) {
        mvaddstr(r.y + y, r.x, "│");
        mvaddstr(r.y + y, r.x + r.w - 1, "│");
    }
    if (left && r.w > 6) {
        pen(p->ui[UI_LABEL], p->bg, A_NORMAL);
        mvaddstr(r.y, r.x + 1, " ");
        int used = put_text(r.y, r.x + 2, r.w - 6, left, 0);
        mvaddstr(r.y, r.x + 2 + used, " ");
    }
    if (right) {
        int w = text_width(right);
        if (w + 4 <= r.w - (left ? text_width(left) + 6 : 2)) {
            pen(p->ui[UI_ACTIVE], p->bg, A_NORMAL);
            int x = r.x + r.w - 3 - w;
            mvaddstr(r.y, x - 1, " ");
            put_text(r.y, x, w, right, 0);
            addstr(" ");
        }
    }
    if (bottom) {
        int w = text_width(bottom);
        if (w + 4 <= r.w) {
            pen(p->ui[UI_DIM], p->bg, A_NORMAL);
            int x = r.x + r.w - 3 - w;
            mvaddstr(r.y + r.h - 1, x - 1, " ");
            put_text(r.y + r.h - 1, x, w, bottom, 0);
            addstr(" ");
        }
    }
}

/* ---- the task list ---- */

static void draw_list(App *a)
{
    const PaletteRGB *p = &a->rgb;
    Rect r = a->lay.list, in = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
    size_t nt = doc_count(&a->doc, LIST_TODAY), nb = doc_count(&a->doc, LIST_BACKLOG);
    draw_box(a, r, "dew ── Today", NULL, NULL);
    if (in.w < 8 || in.h < 1)
        return;

    size_t lines = nt + 1 + nb;
    size_t sel_line = a->sel < nt ? a->sel : a->sel + 1;
    if (sel_line < a->scroll)
        a->scroll = sel_line;
    if (sel_line >= a->scroll + (size_t)in.h)
        a->scroll = sel_line - (size_t)in.h + 1;
    if (lines <= (size_t)in.h)
        a->scroll = 0;

    bool running = a->wave.mode == WAVE_FOCUS || a->wave.mode == WAVE_PAUSED;
    for (int row = 0; row < in.h; row++) {
        size_t line = a->scroll + (size_t)row;
        if (line >= lines)
            break;
        int y = in.y + row;
        if (line == nt) {
            char head[48];
            snprintf(head, sizeof head, "── Backlog (%zu) ", nb);
            pen(p->ui[UI_DIM], p->bg, A_NORMAL);
            int used = put_text(y, in.x, in.w, head, 0);
            for (int x = used; x < in.w; x++)
                addstr("─");
            continue;
        }
        size_t vi = line < nt ? line : line - 1;
        Task *t = doc_view_at(&a->doc, vi, NULL);
        bool sel = vi == a->sel, active = running && t->id == a->active_id;
        uint32_t bg = sel ? p->sel_bg : p->bg;
        uint32_t fg = t->done ? p->ui[UI_DIM]
                    : active  ? p->ui[UI_ACTIVE]
                    : sel     ? p->ui[UI_SEL]
                              : p->ui[UI_TEXT];
        attr_t at = depth < 256 && sel ? A_REVERSE : A_NORMAL; /* few colors: no distinct sel_bg */
        pen(fg, bg, at);
        fill(y, in.x, in.w);
        mvaddstr(y, in.x, active ? "▶" : " ");
        mvaddstr(y, in.x + 2, t->done ? "[x]" : "[ ]");

        char count[24] = "";
        if (t->waves > 0)
            snprintf(count, sizeof count, "%s%d", sprite_text(a->cfg.sprite), t->waves);
        int cw = count[0] ? text_width(count) : 0;
        put_text(y, in.x + 6, in.w - 6 - (cw ? cw + 1 : 0), t->title, 0);
        if (cw) {
            pen(p->ui[UI_COUNT], bg, at);
            put_text(y, in.x + in.w - cw, cw, count, 0);
        }
    }
    if (nt + nb == 0 && in.h > 1) {
        pen(p->ui[UI_DIM], p->bg, A_NORMAL);
        put_text(in.y + in.h - 1, in.x, in.w, "Nothing yet. Press a to add a task.", 0);
    }
}

/* ---- the Life board ---- */

/* The plant (if any) whose box, widened for oscillators, holds board cell (x, y). */
static const GardenBox *box_at(const GardenBox *boxes, size_t n, int x, int y)
{
    for (size_t i = 0; i < n; i++)
        if (x >= boxes[i].x - 3 && x < boxes[i].x + boxes[i].w + 3 && y >= boxes[i].y - 3 &&
            y < boxes[i].y + boxes[i].h + 3)
            return &boxes[i];
    return NULL;
}

static void draw_board(App *a, const Life *l, Rect in, bool idle, const Director *d,
                       const GardenBox *boxes, size_t nboxes)
{
    const PaletteRGB *p = &a->rgb;
    if (in.w <= 0 || in.h <= 0)
        return;
    bool glitch = a->cfg.glitch && d;
    int cols = in.w, rows = in.h;
    size_t ncells = (size_t)(cols * rows);
    uint8_t *obits = xcalloc(ncells, 1), *wbits = xcalloc(ncells, 1), *webits = xcalloc(ncells, 1);
    uint8_t *wrank = xcalloc(ncells, 1);
    uint32_t *ocolor = xcalloc(ncells, sizeof *ocolor);

    if (d && d->sweeping) {
        /* the river: deep water mid-stream, lighter toward the banks, foam at its edge */
        int half = director_river_half(d), edge = (int)d->sweep_x;
        double fade = d->river_fade > 0 ? d->river_fade / 24.0 : 1.0;
        for (int bx = 0; bx < l->w && bx < edge && bx / 2 < cols; bx++) {
            double ry = director_river_y(d, bx);
            for (int by = (int)floor(ry - half - 1); by <= (int)ceil(ry + half + 1); by++) {
                if (by < 0 || by >= l->h || by / 4 >= rows)
                    continue;
                double dist = fabs(by - ry);
                if (dist > half + 1)
                    continue;
                bool bank = dist > half - 0.5, front = bx >= edge - 2;
                long flow = ((bx - d->frame * 2 + by * 3) % 4 + 4) % 4; /* drifts downstream */
                bool dot = front || bank ? rng_unit(&a->fx) < 0.7 : flow != 0;
                if (!dot || rng_unit(&a->fx) > fade)
                    continue;
                uint32_t c = front                ? p->foam[rng_range(&a->fx, 2)]
                           : bank                 ? p->river[3]
                           : dist < half * 0.4    ? p->river[0]
                           : dist < half * 0.8    ? p->river[1]
                                                  : p->river[2];
                if (!front && !bank && rng_unit(&a->fx) < 0.03)
                    c = p->foam[0]; /* sparkle */
                int ci = (by / 4) * cols + bx / 2;
                obits[ci] |= braille_bit(bx & 1, by & 3);
                ocolor[ci] = c;
            }
        }
        /* the wolf runs along the bank, drawn over everything */
        int wx, wy, wf;
        bool wolf = director_wolf(d, &wx, &wy, &wf);
        if (wolf && wolfview_active(a)) { /* real pixels, drawn after the refresh */
            a->wolf.show = true;
            a->wolf.frame = wf;
            a->wolf.x_px = in.x * a->cell_w + wx * a->cell_w / 2;
            a->wolf.row = in.y + wy / 4;
            a->wolf.pane = in;
        } else if (wolf)
            for (int y = 0; y < WOLF_H; y++)
                for (int x = 0; x < WOLF_W; x++) {
                    int px = wolf_pixel(wf, x, y), bx = wx + x, by = wy + y;
                    if (px == PX_NONE || bx < 0 || by < 0 || bx >= l->w || by >= l->h ||
                        bx / 2 >= cols || by / 4 >= rows)
                        continue;
                    int ci = (by / 4) * cols + bx / 2;
                    uint8_t bit = braille_bit(bx & 1, by & 3);
                    if (px == PX_EYE)
                        webits[ci] |= bit;
                    wbits[ci] |= bit;
                    int rank = px == PX_SNOW ? 3 : px == PX_SHADE ? 2 : 1;
                    if (rank > wrank[ci])
                        wrank[ci] = (uint8_t)rank;
                }
    }

    for (int cy = 0; cy < rows; cy++) {
        int off = glitch && d->tear > 0 && rng_unit(&a->fx) < 0.45 ? rng_range(&a->fx, 7) - 3 : 0;
        uint32_t bg = cy % 4 == 1 ? p->band : p->bg;
        for (int cx = 0; cx < cols; cx++) {
            int sx = cx - off;
            uint8_t bits = 0;
            uint32_t fg = p->ui[UI_TEXT];
            if (sx >= 0 && sx < cols) {
                BrailleCell bc = braille_cell(l, sx, cy);
                bits = bc.bits;
                if (bits) {
                    int b = palette_age_bucket(idle && bc.age > 6 ? 6 : bc.age);
                    const GardenBox *gb = nboxes ? box_at(boxes, nboxes, sx * 2, cy * 4 + 1) : NULL;
                    fg = bc.dying                         ? p->dying
                       : gb                               ? p->garden[gb->pair][garden_shade(
                                                                gb, sx * 2, cy * 4 + 1, a->dir.frame)]
                       : d && d->mood == MOOD_PAUSED      ? p->gray[bc.tint][b]
                                                          : p->life[bc.tint][b];
                }
                int ci = cy * cols + sx;
                if (webits[ci]) { /* the eye cell shows just the eye, in red */
                    bits = webits[ci];
                    fg = p->wolf[PX_EYE - 1];
                } else if (wbits[ci]) {
                    bits = wbits[ci];
                    fg = p->wolf[wrank[ci] == 3 ? PX_SNOW - 1 : wrank[ci] == 2 ? PX_SHADE - 1 : PX_DARK - 1];
                } else if (obits[ci]) {
                    bits |= obits[ci];
                    fg = ocolor[ci];
                }
            }
            wchar_t wc[2] = {L' ', 0};
            if (bits && glitch && rng_unit(&a->fx) < 0.005)
                wc[0] = (wchar_t)rune(rng_range(&a->fx, RUNE_COUNT));
            else if (bits)
                wc[0] = (wchar_t)(0x2800 + bits);
            cchar_t cc;
            setcchar(&cc, wc, A_NORMAL, pair_of(fg, bg), NULL);
            mvadd_wch(in.y + cy, in.x + cx, &cc);
        }
    }

    if (glitch && d->tear > 0) {
        for (int k = 0; k < 36; k++) {
            int cx = rng_range(&a->fx, cols), cy = rng_range(&a->fx, rows);
            wchar_t wc[2] = {(wchar_t)rune(rng_range(&a->fx, RUNE_COUNT)), 0};
            cchar_t cc;
            setcchar(&cc, wc, A_NORMAL, pair_of(p->glyph[rng_range(&a->fx, 5)], p->bg), NULL);
            mvadd_wch(in.y + cy, in.x + cx, &cc);
        }
    }

    free(obits);
    free(wbits);
    free(webits);
    free(wrank);
    free(ocolor);
}

static void status_text(App *a, char *out, size_t n)
{
    const char *s = sprite_text(a->cfg.sprite);
    int sec = (int)ceil(wave_remaining(&a->wave, a->now));
    int mm = sec / 60, ss = sec % 60;
    switch (a->wave.mode) {
    case WAVE_FOCUS:
        snprintf(out, n, "%s %02d:%02d focus · wave %d", s, mm, ss, a->wave.waves_today + 1);
        break;
    case WAVE_PAUSED:
        snprintf(out, n, "❚❚ %02d:%02d paused · wave %d", mm, ss, a->wave.waves_today + 1);
        break;
    case WAVE_BREAK:
        snprintf(out, n, "%s %02d:%02d %s", s, mm, ss, a->wave.long_break ? "long break" : "break");
        break;
    default:
        snprintf(out, n, "garden · %d %s today", a->wave.waves_today, s);
    }
}

static void rule_label(Rule r, char *out, size_t n)
{
    char text[32];
    rule_format(r, text, sizeof text);
    snprintf(out, n, "%s %s", text, rule_name(r));
}

static void draw_life(App *a)
{
    Rect r = a->lay.life;
    char status[96], label[64];
    status_text(a, status, sizeof status);
    rule_label(a->dir.life.rule, label, sizeof label);
    draw_box(a, r, NULL, status, label);
    bool garden = a->dir.mood == MOOD_IDLE && !a->dir.sweeping;
    draw_board(a, &a->dir.life, (Rect){r.x + 1, r.y + 1, r.w - 2, r.h - 2},
               a->dir.mood == MOOD_IDLE, &a->dir, a->dir.boxes, garden ? a->dir.nboxes : 0);
}

/* ---- note, keys, overlays ---- */

static void draw_note(App *a)
{
    const PaletteRGB *p = &a->rgb;
    Rect r = a->lay.note;
    draw_box(a, r, "note", NULL, NULL);
    Task *t = doc_view_at(&a->doc, a->sel, NULL);
    if (!t)
        return;
    if (!t->note) {
        pen(p->ui[UI_DIM], p->bg, A_NORMAL);
        put_text(r.y + 1, r.x + 2, r.w - 4, "No note. Press n to write one.", 0);
        return;
    }
    const char *nl = strchr(t->note, '\n');
    Sbuf line;
    sb_init(&line);
    sb_putn(&line, t->note, nl ? (size_t)(nl - t->note) : strlen(t->note));
    if (nl) {
        int more = 0;
        for (const char *c = nl; c; c = strchr(c + 1, '\n'))
            more++;
        sb_printf(&line, "  (+%d more line%s)", more, more == 1 ? "" : "s");
    }
    pen(p->ui[UI_NOTE], p->bg, A_NORMAL);
    put_text(r.y + 1, r.x + 2, r.w - 4, line.buf, 0);
    sb_free(&line);
}

static void draw_keys(App *a)
{
    const PaletteRGB *p = &a->rgb;
    int y = a->lay.keys.y, w = a->lay.keys.w;
    pen(p->ui[UI_TEXT], p->bg, A_NORMAL);
    fill(y, 0, w);

    if (a->prompt == PROMPT_ADD || a->prompt == PROMPT_EDIT) {
        const char *label = a->prompt == PROMPT_EDIT       ? "edit: "
                          : a->add_list == LIST_TODAY      ? "add to Today: "
                                                           : "add to Backlog: ";
        pen(p->ui[UI_LABEL], p->bg, A_NORMAL);
        int lw = put_text(y, 0, w, label, 0);
        char *text = le_text(&a->le), *before = le_before_cursor(&a->le);
        int avail = w - lw - 1, cw = text_width(before);
        int skip = cw > avail ? cw - avail : 0;
        pen(p->ui[UI_TEXT], p->bg, A_NORMAL);
        put_text(y, lw, avail, text, skip);
        a->cur_y = y;
        a->cur_x = lw + cw - skip;
        free(text);
        free(before);
        return;
    }
    if (a->prompt == PROMPT_DELETE) {
        Sbuf q;
        sb_init(&q);
        sb_printf(&q, "Delete \"%s\"? y/n", a->prompt_title ? a->prompt_title : "");
        pen(p->ui[UI_WARN], p->bg, A_NORMAL);
        put_text(y, 0, w, q.buf, 0);
        sb_free(&q);
        return;
    }
    if (a->msg[0] && a->now < a->msg_until) {
        pen(a->msg_warn ? p->ui[UI_WARN] : p->ui[UI_ACTIVE], p->bg, A_NORMAL);
        put_text(y, 0, w, a->msg, 0);
        return;
    }
    static const char *HINTS[][2] = {
        {"a", "add"},  {"e", "edit"},   {"n", "note"}, {"x", "done"},   {"t", "today↔backlog"},
        {"J/K", "move"}, {"␣", "wave"}, {"s", "stop"}, {"r", "rule"}, {"w", "garden"}, {"?", "help"}, {"q", "quit"},
    };
    int x = 0;
    for (size_t i = 0; i < sizeof HINTS / sizeof *HINTS; i++) {
        int need = text_width(HINTS[i][0]) + 1 + text_width(HINTS[i][1]) + 2;
        if (x + need > w)
            break;
        pen(p->ui[UI_LABEL], p->bg, A_NORMAL);
        x += put_text(y, x, w - x, HINTS[i][0], 0) + 1;
        pen(p->ui[UI_DIM], p->bg, A_NORMAL);
        x += put_text(y, x, w - x, HINTS[i][1], 0) + 2;
    }
}

static void draw_help(App *a)
{
    static const char *HELP[][2] = {
        {"j k ↓ ↑", "move"},
        {"g G", "top / bottom"},
        {"Tab", "jump between Today and Backlog"},
        {"a", "add a task"},
        {"e", "edit the title"},
        {"n", "edit the note in $EDITOR"},
        {"x", "done / not done"},
        {"t", "move to Today / Backlog"},
        {"J K", "move down / up"},
        {"d", "delete"},
        {"Space", "start, pause or resume a wave"},
        {"s", "finish the wave now, or end the break"},
        {"S", "abandon the wave (not counted)"},
        {"r", "next simulation for waves"},
        {"w", "today's garden"},
        {"q", "quit (a running wave resumes next time)"},
    };
    const PaletteRGB *p = &a->rgb;
    int n = (int)(sizeof HELP / sizeof *HELP);
    int w = a->cols - 4 < 54 ? a->cols - 4 : 54, h = n + 4;
    if (h > a->rows - 2)
        h = a->rows - 2;
    Rect r = {(a->cols - w) / 2, (a->rows - h) / 2, w, h};
    pen(p->ui[UI_TEXT], p->bg, A_NORMAL);
    for (int y = 0; y < r.h; y++)
        fill(r.y + y, r.x, r.w);
    draw_box(a, r, "keys", NULL, "any key closes");
    for (int i = 0; i < n && i < r.h - 4; i++) {
        pen(p->ui[UI_LABEL], p->bg, A_NORMAL);
        put_text(r.y + 2 + i, r.x + 2, 9, HELP[i][0], 0);
        pen(p->ui[UI_TEXT], p->bg, A_NORMAL);
        put_text(r.y + 2 + i, r.x + 12, r.w - 14, HELP[i][1], 0);
    }
}

static void draw_garden_view(App *a)
{
    const PaletteRGB *p = &a->rgb;
    char title[64];
    snprintf(title, sizeof title, "garden · %s · %zu %s", a->today, a->garden.n,
             a->garden.n == 1 ? "wave" : "waves");
    Rect r = {0, 0, a->cols, a->rows - 1};
    draw_box(a, r, title, NULL, NULL);
    draw_board(a, &a->gview, (Rect){1, 1, r.w - 2, r.h - 2}, true, NULL, a->gboxes, a->ngboxes);
    pen(p->ui[UI_DIM], p->bg, A_NORMAL);
    fill(a->rows - 1, 0, a->cols);
    put_text(a->rows - 1, 0, a->cols, "any key returns", 0);
}

void render(App *a)
{
    const PaletteRGB *p = &a->rgb;
    cchar_t blank;
    setcchar(&blank, L" ", A_NORMAL, pair_of(p->ui[UI_TEXT], p->bg), NULL);
    wbkgrndset(stdscr, &blank);
    erase();
    a->wolf.show = false;
    curs_set(0);

    if (a->lay.too_small) {
        pen(p->ui[UI_TEXT], p->bg, A_NORMAL);
        mvaddstr(a->rows / 2, 0, "dew needs 30x8");
    } else if (a->garden_view) {
        draw_garden_view(a);
    } else {
        draw_list(a);
        if (a->lay.show_life)
            draw_life(a);
        draw_note(a);
        draw_keys(a);
        if (a->help)
            draw_help(a);
        if (a->prompt == PROMPT_ADD || a->prompt == PROMPT_EDIT) {
            curs_set(1);
            move(a->cur_y, a->cur_x);
        }
    }
    if (a->help || a->prompt != PROMPT_NONE)
        a->wolf.show = false; /* the image would cover them */
    wolfview_before(a);
    refresh();
    wolfview_after(a);
}
