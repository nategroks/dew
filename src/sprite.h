#ifndef DEW_SPRITE_H
#define DEW_SPRITE_H

#include <stdint.h>

/* The white wolf that runs along the river, and the Elder Futhark runes. */

#define WOLF_W 32
#define WOLF_H 14
#define WOLF_FRAMES 4
#define RUNE_COUNT 24

enum { PX_NONE, PX_SNOW, PX_SHADE, PX_EYE, PX_DARK };

/* Pixel kind at (x, y) of a run-cycle frame (frame wraps; out of range is PX_NONE). Faces right. */
int wolf_pixel(int frame, int x, int y);
uint32_t rune(int i); /* the i-th Elder Futhark rune, i taken mod RUNE_COUNT */

#endif
