#ifndef DEW_SPRITE_H
#define DEW_SPRITE_H

#include <stdint.h>

/* The wolves that run across the Life board, and the Elder Futhark runes. */

#include "wolfart.h"

/* A wolf's size on the Life board, in braille dots (the art's aspect). */
#define WOLF_W 48
#define WOLF_H 29
#define WOLF_FRAMES WOLF_ART_FRAMES
#define WOLF_ZONES 3 /* tail, body, head: a coat can change color along the wolf */
#define RUNE_COUNT 24

enum { PX_NONE, PX_SNOW, PX_SHADE, PX_EYE, PX_DARK };

/* The snow wolf runs with the break river; the others run when a task is done. */
enum { WOLF_SNOW, WOLF_FROST, WOLF_EMBER, WOLF_AURORA, WOLF_SHADOW, WOLF_DUSK, WOLF_KINDS };

const char *wolf_name(int kind); /* "frost" */
int wolf_cycle(int kind);        /* the run cycle it uses: 0 gallop, 1 trot */
/* Pixel kind at (x, y) of a run-cycle frame (frame wraps; out of range is PX_NONE). Faces right.
   Unknown kinds are drawn as the snow wolf. */
int wolf_pixel(int kind, int frame, int x, int y);
/* The color of art pixel `art` (ART_SNOW .. ART_EAR) in this wolf's coat, `along` its length
   from 0 (tail) to 1 (nose), from the 16 Nord colors. */
uint32_t wolf_rgb(const uint32_t nord[16], int kind, int art, double along);
/* Draws a frame into a w x h RGBA canvas (cleared to transparent), scaled up by the largest
   whole number that fits (at least 1), centered and standing on the bottom. Returns the scale. */
int wolf_canvas(const uint32_t nord[16], int kind, int frame, int w, int h, uint8_t *rgba);
uint32_t rune(int i); /* the i-th Elder Futhark rune, i taken mod RUNE_COUNT */

#endif
