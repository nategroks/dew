#ifndef DEW_SPRITE_H
#define DEW_SPRITE_H

#include <stdint.h>

/* The white wolf that runs along the river, and the Elder Futhark runes. */

#include "wolfart.h"

/* The wolf's size on the Life board, in braille dots (the art's aspect). */
#define WOLF_W 48
#define WOLF_H 28
#define WOLF_FRAMES WOLF_ART_FRAMES
#define RUNE_COUNT 24

enum { PX_NONE, PX_SNOW, PX_SHADE, PX_EYE, PX_DARK };

/* Pixel kind at (x, y) of a run-cycle frame (frame wraps; out of range is PX_NONE). Faces right. */
int wolf_pixel(int frame, int x, int y);
/* Draws a frame into a w x h RGBA canvas (cleared to transparent), scaled up by the largest
   whole number that fits (at least 1), centered and standing on the bottom. Returns the scale. */
int wolf_canvas(int frame, int w, int h, uint8_t *rgba);
uint32_t rune(int i); /* the i-th Elder Futhark rune, i taken mod RUNE_COUNT */

#endif
