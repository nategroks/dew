#include "gfx.h"

#include <stdlib.h>
#include <string.h>

static bool has_prefix(const char *s, const char *pre)
{
    return s && strncmp(s, pre, strlen(pre)) == 0;
}

GfxMode gfx_pick(GfxMode setting, const char *term, const char *term_program,
                 const char *kitty_window_id, const char *tmux)
{
    if (setting != GFX_AUTO)
        return setting;
    if (tmux && *tmux)
        return GFX_BRAILLE;
    if (has_prefix(term, "xterm-kitty") || has_prefix(term, "xterm-ghostty") ||
        (kitty_window_id && *kitty_window_id) ||
        (term_program && strcmp(term_program, "ghostty") == 0))
        return GFX_KITTY;
    if (has_prefix(term, "foot"))
        return GFX_SIXEL;
    return GFX_BRAILLE;
}

static void base64(Sbuf *b, const uint8_t *p, size_t n)
{
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16;
        if (i + 1 < n)
            v |= (uint32_t)p[i + 1] << 8;
        if (i + 2 < n)
            v |= p[i + 2];
        char q[4] = {A[v >> 18], A[(v >> 12) & 63], i + 1 < n ? A[(v >> 6) & 63] : '=',
                     i + 2 < n ? A[v & 63] : '='};
        sb_putn(b, q, 4);
    }
}

void kitty_upload(Sbuf *b, unsigned id, const uint8_t *rgba, int w, int h)
{
    Sbuf enc;
    sb_init(&enc);
    base64(&enc, rgba, (size_t)w * (size_t)h * 4);
    const size_t CHUNK = 4096;
    for (size_t off = 0; off < enc.len || off == 0; off += CHUNK) {
        size_t n = enc.len - off < CHUNK ? enc.len - off : CHUNK;
        int more = off + n < enc.len;
        if (off == 0)
            sb_printf(b, "\033_Ga=t,f=32,s=%d,v=%d,i=%u,q=2,m=%d;", w, h, id, more);
        else
            sb_printf(b, "\033_Gm=%d,q=2;", more);
        sb_putn(b, enc.buf + off, n);
        sb_puts(b, "\033\\");
        if (!more)
            break;
    }
    sb_free(&enc);
}

void kitty_place(Sbuf *b, unsigned id, int row, int col, int xoff, int src_x, int src_w,
                 int src_h)
{
    sb_printf(b, "\0337\033[%d;%dH\033_Ga=p,i=%u,p=1,X=%d,x=%d,y=0,w=%d,h=%d,C=1,z=1,q=2\033\\\0338",
              row + 1, col + 1, id, xoff, src_x, src_w, src_h);
}

void kitty_unplace(Sbuf *b, unsigned id)
{
    sb_printf(b, "\033_Ga=d,d=i,i=%u,q=2\033\\", id);
}

void kitty_free(Sbuf *b, unsigned id)
{
    sb_printf(b, "\033_Ga=d,d=I,i=%u,q=2\033\\", id);
}

static void sixel_run(Sbuf *b, char c, int n)
{
    if (n >= 4) {
        sb_printf(b, "!%d%c", n, c);
        return;
    }
    while (n-- > 0)
        sb_putn(b, &c, 1);
}

void sixel_encode(Sbuf *b, const uint8_t *rgba, int stride, int x0, int y0, int w, int h)
{
    sb_printf(b, "\033P0;1;0q\"1;1;%d;%d", w, h);
    /* palette: opaque colors in order of first appearance */
    uint32_t pal[256];
    int npal = 0;
    int *idx = xmalloc((size_t)(w > 0 && h > 0 ? w * h : 1) * sizeof *idx);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *p = rgba + 4 * ((size_t)(y0 + y) * (size_t)stride + (size_t)(x0 + x));
            int k = -1;
            if (p[3]) {
                uint32_t c = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
                for (k = 0; k < npal && pal[k] != c; k++) {
                }
                if (k == npal) {
                    if (npal == 256) {
                        k = 255; /* more colors than sixel can name: share the last */
                    } else {
                        pal[npal++] = c;
                        sb_printf(b, "#%d;2;%u;%u;%u", k + 1, (c >> 16) * 100 / 255,
                                  ((c >> 8) & 0xFF) * 100 / 255, (c & 0xFF) * 100 / 255);
                    }
                }
            }
            idx[y * w + x] = k;
        }

    char *row = xmalloc((size_t)(w > 0 ? w : 1));
    int newlines = 0; /* band breaks, written only when something follows them */
    for (int band = 0; band * 6 < h; band++, newlines++) {
        for (int k = 0; k < npal; k++) {
            int last = -1;
            for (int x = 0; x < w; x++) {
                int bits = 0;
                for (int r = 0; r < 6 && band * 6 + r < h; r++)
                    if (idx[(band * 6 + r) * w + x] == k)
                        bits |= 1 << r;
                row[x] = (char)(63 + bits);
                if (bits)
                    last = x;
            }
            if (last < 0)
                continue;
            for (; newlines > 0; newlines--)
                sb_puts(b, "-");
            sb_printf(b, "#%d", k + 1);
            for (int x = 0; x <= last;) {
                int n = 1;
                while (x + n <= last && row[x + n] == row[x])
                    n++;
                sixel_run(b, row[x], n);
                x += n;
            }
            sb_puts(b, "$");
        }
    }
    sb_puts(b, "\033\\");
    free(row);
    free(idx);
}
