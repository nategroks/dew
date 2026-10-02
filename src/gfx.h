#ifndef DEW_GFX_H
#define DEW_GFX_H

#include <stdint.h>

#include "util.h"

/* How the wolf is drawn: braille dots everywhere, or real pixels through
   kitty's graphics protocol (kitty, ghostty) or sixel (foot). */
typedef enum { GFX_AUTO, GFX_BRAILLE, GFX_KITTY, GFX_SIXEL } GfxMode;

/* Resolves GFX_AUTO from the environment; never asks the terminal, so no
   reply can land in the keyboard input. Any other setting is returned as is. */
GfxMode gfx_pick(GfxMode setting, const char *term, const char *term_program,
                 const char *kitty_window_id, const char *tmux);

/* kitty graphics commands; q=2 so the terminal never answers. */
void kitty_upload(Sbuf *b, unsigned id, const uint8_t *rgba, int w, int h);
/* Shows image id at screen cell (row, col) shifted xoff pixels right, using
   source pixels x src_x .. src_x+src_w and rows 0 .. src_h. Keeps the cursor. */
void kitty_place(Sbuf *b, unsigned id, int row, int col, int xoff, int src_x, int src_w,
                 int src_h);
void kitty_unplace(Sbuf *b, unsigned id); /* hide; keep the pixels */
void kitty_free(Sbuf *b, unsigned id);    /* hide and forget */

/* A sixel image of the w x h pixels at (x0, y0) of an RGBA image `stride`
   pixels wide. Fully transparent pixels stay transparent. */
void sixel_encode(Sbuf *b, const uint8_t *rgba, int stride, int x0, int y0, int w, int h);

#endif
