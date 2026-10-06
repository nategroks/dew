#include "wolfview.h"

#include "gfx.h"
#include "sprite.h"

#include <curses.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FIRST_ID 0x6477u /* kitty image ids: FIRST_ID + kind * WOLF_FRAMES + frame */
#define COLS_ (WOLF_W / 2)
#define ROWS_ (WOLF_H / 4)

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
    a->wolf_sync = wolfview_active(a) && (a->wolf.show || a->wolf_prev.shown);
    if (!a->wolf_sync)
        return;
    Sbuf b;
    sb_init(&b);
    sb_puts(&b, "\033[?2026h"); /* hold the frame until the wolf is drawn too */
    emit(&b);
    sb_free(&b);
    /* sixels stay until text covers them: have curses rewrite where the wolf was */
    if (a->gfx == GFX_SIXEL && a->wolf_prev.shown) {
        int y = a->wolf_prev.row, n = a->wolf_prev.rows;
        if (y < 0) {
            n += y;
            y = 0;
        }
        if (y + n > a->rows)
            n = a->rows - y;
        if (n > 0)
            wredrawln(stdscr, y, n);
        a->wolf_prev.shown = false;
    }
}

/* kitty keeps each coat's frames once it has been seen, until the cells change size */
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

void wolfview_after(App *a)
{
    if (!a->wolf_sync)
        return;
    a->wolf_sync = false;
    Sbuf b;
    sb_init(&b);
    int cw = a->cell_w, ch = a->cell_h, pw = COLS_ * cw, ph = ROWS_ * ch;

    unsigned id = image_id(a->wolf.kind, a->wolf.frame);
    if (a->gfx == GFX_KITTY && a->wolf_prev.shown && (!a->wolf.show || a->wolf_prev.id != id)) {
        kitty_unplace(&b, a->wolf_prev.id);
        a->wolf_prev.shown = false;
    }

    if (a->wolf.show) {
        /* the wolf in screen pixels, clipped to the Life pane */
        Rect pane = a->wolf.pane;
        int x = a->wolf.x_px, left = pane.x * cw, right = (pane.x + pane.w) * cw;
        if (a->gfx == GFX_SIXEL)
            x = (x >= 0 ? x / cw : -((-x + cw - 1) / cw)) * cw; /* sixels start on a cell */
        int x0 = x < left ? left : x, x1 = x + pw > right ? right : x + pw;
        if (x1 - x0 >= cw) {
            int kind = a->wolf.kind;
            if (a->gfx == GFX_KITTY) {
                if (a->wolf_up_w[kind] != pw || a->wolf_up_h[kind] != ph)
                    upload(a, kind, pw, ph, &b);
                kitty_place(&b, id, a->wolf.row, x0 / cw, x0 % cw, x0 - x, x1 - x0, ph);
            } else {
                uint8_t *px = xmalloc((size_t)pw * (size_t)ph * 4);
                wolf_canvas(a->cfg.nord, kind, a->wolf.frame, pw, ph, px);
                sb_printf(&b, "\0337\033[%d;%dH", a->wolf.row + 1, x0 / cw + 1);
                sixel_encode(&b, px, pw, x0 - x, 0, x1 - x0, ph);
                sb_puts(&b, "\0338");
                free(px);
            }
            a->wolf_prev.shown = true;
            a->wolf_prev.id = id;
            a->wolf_prev.row = a->wolf.row;
            a->wolf_prev.rows = ROWS_ + 1; /* a sixel's last band can spill a row */
        }
    }
    sb_puts(&b, "\033[?2026l");
    emit(&b);
    sb_free(&b);
}

void wolfview_hide(App *a)
{
    if (a->gfx == GFX_KITTY && a->wolf_prev.shown) {
        Sbuf b;
        sb_init(&b);
        kitty_unplace(&b, a->wolf_prev.id);
        emit(&b);
        sb_free(&b);
    }
    a->wolf_prev.shown = false;
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
    emit(&b);
    sb_free(&b);
}
