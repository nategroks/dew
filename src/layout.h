#ifndef DEW_LAYOUT_H
#define DEW_LAYOUT_H

#include <stdbool.h>

#define MIN_COLS 30
#define MIN_ROWS 8
#define LIFE_MIN_COLS 80
#define LIFE_MIN_ROWS 24

typedef struct {
    int x, y, w, h;
} Rect;

/* Boxes include their border; the keys line has none. */
typedef struct {
    bool too_small, show_life;
    Rect list, life, note, keys;
} Layout;

Layout layout_compute(int cols, int rows);

#endif
