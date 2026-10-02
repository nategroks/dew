#include "braille.h"

uint8_t braille_bit(int dx, int dy)
{
    static const uint8_t BITS[2][4] = {{0x01, 0x02, 0x04, 0x40}, {0x08, 0x10, 0x20, 0x80}};
    return BITS[dx & 1][dy & 3];
}

BrailleCell braille_cell(const Life *l, int cx, int cy)
{
    BrailleCell c = {0, 0, UINT16_MAX, true};
    bool any_on = false;
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 2; dx++) {
            int x = cx * 2 + dx, y = cy * 4 + dy;
            if (x >= l->w || y >= l->h)
                continue;
            uint8_t s = life_get(l, x, y);
            if (s == CELL_OFF)
                continue;
            c.bits |= braille_bit(dx, dy);
            if (s == CELL_ON) {
                uint16_t a = life_age(l, x, y);
                if (!any_on || a < c.age) {
                    c.age = a;
                    c.tint = life_tint(l, x, y);
                }
                any_on = true;
            }
        }
    if (any_on || !c.bits)
        c.dying = false;
    if (!any_on)
        c.age = 0;
    return c;
}
