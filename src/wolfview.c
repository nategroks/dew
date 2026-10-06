#include "wolfview.h"

#include "gfx.h"
#include "runes.h"
#include "sprite.h"

#include <curses.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FIRST_ID 0x6477u /* kitty image ids: FIRST_ID + kind * WOLF_FRAMES + frame */
#define RUNE_ID 0x6577u  /* and RUNE_ID + rune for the runes the wolves chase */
#define COLS_ (WOLF_W / 2)
#define ROWS_ (WOLF_H / 4)
#define RCOLS ((RUNE_ART_W + 1) / 2)
#define RROWS ((RUNE_ART_H + 3) / 4)

static void emit(Sbuf *b)
{
    for (size_t off = 0; off < b->len;) {
        ssize_t n = write(STDOUT_FILENO, b->buf + off, b->len - off);
        if (n <= 0)
            break;
        off += (size_t)n;
    }
    sb_free(b);
    sb_init(b);
}

static unsigned image_id(int kind, int frame)
{
    return FIRST_ID + (unsigned)(kind * WOLF_FRAMES + frame);
}

void wolfview_init(App *a)
{
    a->gfx = gfx_pick(a->cfg.wolf_graphics, getenv("TERM"), getenv("TERM_PROGRAM"),
                      getenv("KITTY_WINDOW_ID"), getenv("TMUX"));
}

void wolfview_measure(App *a)
{
    struct winsize ws;
    a->cell_w = a->cell_h = 0;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        a->cell_w = ws.ws_xpixel / ws.ws_col;
        a->cell_h = ws.ws_ypixel / ws.ws_row;
    }
}

bool wolfview_active(const App *a)
{
    return a->gfx != GFX_BRAILLE && a->cell_w >= 2 && a->cell_h >= 4;
}

void wolfview_before(App *a)
{
    bool prev = a->wolf_prev.shown || a->wolf_prev.rune_shown;
    a->wolf_sync = wolfview_active(a) && (a->wolf.show || prev);
    if (!a->wolf_sync)
        return;
    Sbuf b;
    sb_init(&b);
    sb_puts(&b, "\033[?2026h"); /* hold the frame until the wolf is drawn too */
    emit(&b);
    sb_free(&b);
    /* sixels stay until text covers them: have curses rewrite where the wolf and rune were */
    if (a->gfx == GFX_SIXEL && prev) {
        int y = a->wolf_prev.row, n = a->wolf_prev.rows;
        if (y < 0) {
            n += y;
            y = 0;
        }
        if (y + n > a->rows)
            n = a->rows - y;
        if (n > 0)
            wredrawln(stdscr, y, n);
        a->wolf_prev.shown = a->wolf_prev.rune_shown = false;
    }
}

/* kitty keeps each coat's frames, and each rune, once it has been seen, until the cells
   change size */
static void upload(App *a, int kind, int pw, int ph, Sbuf *b)
{
    uint8_t *px = xmalloc((size_t)pw * (size_t)ph * 4);
    for (int f = 0; f < WOLF_FRAMES; f++) {
        if (a->wolf_up_w[kind])
            kitty_free(b, image_id(kind, f));
        wolf_canvas(a->cfg.nord, kind, f, pw, ph, px);
        kitty_upload(b, image_id(kind, f), px, pw, ph);
    }
    free(px);
    a->wolf_up_w[kind] = pw;
    a->wolf_up_h[kind] = ph;
}

static void upload_rune(App *a, int rune, int pw, int ph, Sbuf *b)
{
    uint8_t *px = xmalloc((size_t)pw * (size_t)ph * 4);
    if (a->rune_up_w[rune])
        kitty_free(b, RUNE_ID + (unsigned)rune);
    rune_canvas(a->cfg.nord, rune, pw, ph, px);
    kitty_upload(b, RUNE_ID + (unsigned)rune, px, pw, ph);
    free(px);
    a->rune_up_w[rune] = pw;
    a->rune_up_h[rune] = ph;
}

/* Shows a wolf frame (rune < 0) or a rune, pw x ph pixels with its left edge at screen pixel x
   and its top at screen row `row`, clipped to the Life pane. False when none of it is there. */
static bool put_image(App *a, Sbuf *b, int rune, int x, int row, int pw, int ph)
{
    int cw = a->cell_w, kind = a->wolf.kind, frame = a->wolf.frame;
    Rect pane = a->wolf.pane;
    int left = pane.x * cw, right = (pane.x + pane.w) * cw;
    if (a->gfx == GFX_SIXEL)
        x = (x >= 0 ? x / cw : -((-x + cw - 1) / cw)) * cw; /* sixels start on a cell */
    int x0 = x < left ? left : x, x1 = x + pw > right ? right : x + pw;
    if (x1 - x0 < cw)
        return false;
    if (a->gfx == GFX_KITTY) {
        unsigned id;
        if (rune >= 0) {
            if (a->rune_up_w[rune] != pw || a->rune_up_h[rune] != ph)
                upload_rune(a, rune, pw, ph, b);
            id = RUNE_ID + (unsigned)rune;
        } else {
            if (a->wolf_up_w[kind] != pw || a->wolf_up_h[kind] != ph)
                upload(a, kind, pw, ph, b);
            id = image_id(kind, frame);
        }
        kitty_place(b, id, row, x0 / cw, x0 % cw, x0 - x, x1 - x0, ph);
    } else {
        uint8_t *px = xmalloc((size_t)pw * (size_t)ph * 4);
        if (rune >= 0)
            rune_canvas(a->cfg.nord, rune, pw, ph, px);
        else
            wolf_canvas(a->cfg.nord, kind, frame, pw, ph, px);
        sb_printf(b, "\0337\033[%d;%dH", row + 1, x0 / cw + 1);
        sixel_encode(b, px, pw, x0 - x, 0, x1 - x0, ph);
        sb_puts(b, "\0338");
        free(px);
    }
    return true;
}

/* Grows the remembered screen rows to cover rows .. rows+n. */
static void cover_rows(App *a, bool first, int row, int n)
{
    if (first) {
        a->wolf_prev.row = row;
        a->wolf_prev.rows = n;
        return;
    }
    int end = a->wolf_prev.row + a->wolf_prev.rows;
    if (row < a->wolf_prev.row)
        a->wolf_prev.row = row;
    if (row + n > end)
        end = row + n;
    a->wolf_prev.rows = end - a->wolf_prev.row;
}

void wolfview_after(App *a)
{
    if (!a->wolf_sync)
        return;
    a->wolf_sync = false;
    Sbuf b;
    sb_init(&b);
    int cw = a->cell_w, ch = a->cell_h;
    int rune = a->wolf.show && a->wolf.rune >= 0 && a->wolf.rune < RUNE_ART_COUNT ? a->wolf.rune
                                                                                 : RUNE_NONE;
    unsigned id = image_id(a->wolf.kind, a->wolf.frame), rid = RUNE_ID + (unsigned)(rune < 0 ? 0 : rune);

    /* kitty keeps placements until told: take down what isn't drawn again in the same place */
    bool wolf_on = false, rune_on = false;
    if (a->wolf.show) {
        wolf_on = put_image(a, &b, RUNE_NONE, a->wolf.x_px, a->wolf.row, COLS_ * cw, ROWS_ * ch);
        if (rune != RUNE_NONE)
            rune_on = put_image(a, &b, rune, a->wolf.rune_x_px, a->wolf.rune_row, RCOLS * cw, RROWS * ch);
    }
    if (a->gfx == GFX_KITTY) {
        if (a->wolf_prev.shown && (!wolf_on || a->wolf_prev.id != id))
            kitty_unplace(&b, a->wolf_prev.id);
        if (a->wolf_prev.rune_shown && (!rune_on || a->wolf_prev.rune_id != rid))
            kitty_unplace(&b, a->wolf_prev.rune_id);
    }
    a->wolf_prev.shown = wolf_on;
    a->wolf_prev.rune_shown = rune_on;
    if (wolf_on) {
        a->wolf_prev.id = id;
        cover_rows(a, true, a->wolf.row, ROWS_ + 1); /* a sixel's last band can spill a row */
    }
    if (rune_on) {
        a->wolf_prev.rune_id = rid;
        cover_rows(a, !wolf_on, a->wolf.rune_row, RROWS + 1);
    }
    sb_puts(&b, "\033[?2026l");
    emit(&b);
    sb_free(&b);
}

void wolfview_hide(App *a)
{
    if (a->gfx == GFX_KITTY && (a->wolf_prev.shown || a->wolf_prev.rune_shown)) {
        Sbuf b;
        sb_init(&b);
        if (a->wolf_prev.shown)
            kitty_unplace(&b, a->wolf_prev.id);
        if (a->wolf_prev.rune_shown)
            kitty_unplace(&b, a->wolf_prev.rune_id);
        emit(&b);
        sb_free(&b);
    }
    a->wolf_prev.shown = a->wolf_prev.rune_shown = false;
}

void wolfview_free(App *a)
{
    wolfview_hide(a);
    if (a->gfx != GFX_KITTY)
        return;
    Sbuf b;
    sb_init(&b);
    for (int k = 0; k < WOLF_KINDS; k++) {
        if (!a->wolf_up_w[k])
            continue;
        for (int f = 0; f < WOLF_FRAMES; f++)
            kitty_free(&b, image_id(k, f));
        a->wolf_up_w[k] = a->wolf_up_h[k] = 0;
    }
    for (int r = 0; r < RUNE_ART_COUNT; r++) {
        if (!a->rune_up_w[r])
            continue;
        kitty_free(&b, RUNE_ID + (unsigned)r);
        a->rune_up_w[r] = a->rune_up_h[r] = 0;
    }
    emit(&b);
    sb_free(&b);
}
