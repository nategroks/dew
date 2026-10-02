#ifndef DEW_BRAILLE_H
#define DEW_BRAILLE_H

#include "life.h"

#include <stdbool.h>
#include <stdint.h>

/* One terminal cell shows a 2x4 block of Life cells as a braille glyph. */

typedef struct {
    uint8_t bits;  /* braille dot bits; the glyph is U+2800 + bits */
    uint8_t tint;  /* of the youngest live dot */
    uint16_t age;  /* of the youngest live dot */
    bool dying;    /* only dying dots (Brian's Brain) */
} BrailleCell;

uint8_t braille_bit(int dx, int dy); /* dx 0..1, dy 0..3 */
BrailleCell braille_cell(const Life *l, int cx, int cy);

#endif
