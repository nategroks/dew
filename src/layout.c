#include "layout.h"

Layout layout_compute(int cols, int rows)
{
    Layout l = {0};
    if (cols < MIN_COLS || rows < MIN_ROWS) {
        l.too_small = true;
        return l;
    }
    l.show_life = cols >= LIFE_MIN_COLS && rows >= LIFE_MIN_ROWS;
    l.keys = (Rect){0, rows - 1, cols, 1};
    l.note = (Rect){0, rows - 4, cols, 3};
    int top = rows - 4;
    if (l.show_life) {
        int lw = cols * 2 / 5;
        l.list = (Rect){0, 0, lw, top};
        l.life = (Rect){lw, 0, cols - lw, top};
    } else {
        l.list = (Rect){0, 0, cols, top};
    }
    return l;
}
