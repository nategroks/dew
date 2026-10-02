#ifndef DEW_PATTERNS_H
#define DEW_PATTERNS_H

#include "life.h"

#include <stdbool.h>
#include <stdint.h>

#define PATTERN_MAX_CELLS 64

typedef struct {
    int w, h, n;
    int8_t xy[2 * PATTERN_MAX_CELLS]; /* x0, y0, x1, y1, ... */
} Pattern;

/*
 * Names: glider (moves +x +y), lwss (moves -x), rpent, acorn, diehard,
 * block, beehive, loaf, boat, tub, blinker, toad, beacon, penta
 * (pentadecathlon), pulsar.
 */
bool pattern_get(const char *name, Pattern *out); /* false for an unknown name */
void pattern_flip(Pattern *p, bool flip_x, bool flip_y);
int pattern_period(const char *name); /* Conway oscillation period: 1 for still lifes */
void pattern_stamp(Life *l, const Pattern *p, int x, int y, uint8_t tint, uint16_t age);

#endif
