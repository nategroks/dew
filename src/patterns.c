#include "patterns.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *rows[13];
} PatternDef;

static const PatternDef DEFS[] = {
    {"glider", {".O.", "..O", "OOO"}},
    {"lwss", {".O..O", "O....", "O...O", ".OOOO"}},
    {"rpent", {".OO", "OO.", ".O."}},
    {"acorn", {".O.....", "...O...", "OO..OOO"}},
    {"diehard", {"......O.", "OO......", ".O...OOO"}},
    {"block", {"OO", "OO"}},
    {"beehive", {".OO.", "O..O", ".OO."}},
    {"loaf", {".OO.", "O..O", ".O.O", "..O."}},
    {"boat", {"OO.", "O.O", ".O."}},
    {"tub", {".O.", "O.O", ".O."}},
    {"blinker", {"OOO"}},
    {"toad", {".OOO", "OOO."}},
    {"beacon", {"OO..", "OO..", "..OO", "..OO"}},
    {"penta", {"..O....O..", "OO.OOOO.OO", "..O....O.."}},
    {"pulsar",
     {"..OOO...OOO..", ".............", "O....O.O....O", "O....O.O....O", "O....O.O....O",
      "..OOO...OOO..", ".............", "..OOO...OOO..", "O....O.O....O", "O....O.O....O",
      "O....O.O....O", ".............", "..OOO...OOO.."}},
};

bool pattern_get(const char *name, Pattern *out)
{
    for (size_t d = 0; d < sizeof DEFS / sizeof *DEFS; d++) {
        if (strcmp(DEFS[d].name, name) != 0)
            continue;
        memset(out, 0, sizeof *out);
        for (int y = 0; y < 13 && DEFS[d].rows[y]; y++) {
            const char *row = DEFS[d].rows[y];
            int len = (int)strlen(row);
            if (len > out->w)
                out->w = len;
            out->h = y + 1;
            for (int x = 0; x < len; x++)
                if (row[x] == 'O' && out->n < PATTERN_MAX_CELLS) {
                    out->xy[2 * out->n] = (int8_t)x;
                    out->xy[2 * out->n + 1] = (int8_t)y;
                    out->n++;
                }
        }
        return true;
    }
    return false;
}

int pattern_period(const char *name)
{
    static const struct {
        const char *name;
        int period;
    } P[] = {{"blinker", 2}, {"toad", 2}, {"beacon", 2}, {"pulsar", 3}, {"penta", 15}};
    for (size_t i = 0; i < sizeof P / sizeof *P; i++)
        if (strcmp(P[i].name, name) == 0)
            return P[i].period;
    return 1;
}

void pattern_flip(Pattern *p, bool flip_x, bool flip_y)
{
    for (int i = 0; i < p->n; i++) {
        if (flip_x)
            p->xy[2 * i] = (int8_t)(p->w - 1 - p->xy[2 * i]);
        if (flip_y)
            p->xy[2 * i + 1] = (int8_t)(p->h - 1 - p->xy[2 * i + 1]);
    }
}

void pattern_stamp(Life *l, const Pattern *p, int x, int y, uint8_t tint, uint16_t age)
{
    for (int i = 0; i < p->n; i++)
        life_set(l, x + p->xy[2 * i], y + p->xy[2 * i + 1], CELL_ON, tint, age);
}
