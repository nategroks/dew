#include "life.h"

#include "util.h"

#include <stdlib.h>
#include <string.h>

#define ABSORB 3

void life_init(Life *l, int w, int h)
{
    l->w = w > 0 ? w : 0;
    l->h = h > 0 ? h : 0;
    l->sw = l->w + 2 * LIFE_MARGIN;
    l->sh = l->h + 2 * LIFE_MARGIN;
    size_t n = (size_t)l->sw * (size_t)l->sh;
    l->st = xcalloc(n, 1);
    l->st2 = xcalloc(n, 1);
    l->age = xcalloc(n, sizeof *l->age);
    l->age2 = xcalloc(n, sizeof *l->age2);
    l->tint = xcalloc(n, 1);
    l->tint2 = xcalloc(n, 1);
    l->rule = RULE_CONWAY;
}

void life_free(Life *l)
{
    free(l->st);
    free(l->st2);
    free(l->age);
    free(l->age2);
    free(l->tint);
    free(l->tint2);
    memset(l, 0, sizeof *l);
}

static int in_sim(const Life *l, int x, int y)
{
    return x >= -LIFE_MARGIN && y >= -LIFE_MARGIN && x < l->w + LIFE_MARGIN &&
           y < l->h + LIFE_MARGIN;
}

static size_t idx(const Life *l, int x, int y)
{
    return (size_t)(y + LIFE_MARGIN) * (size_t)l->sw + (size_t)(x + LIFE_MARGIN);
}

void life_resize(Life *l, int w, int h)
{
    Life n;
    life_init(&n, w, h);
    n.rule = l->rule;
    for (int y = 0; y < n.h && y < l->h; y++)
        for (int x = 0; x < n.w && x < l->w; x++) {
            size_t a = idx(l, x, y), b = idx(&n, x, y);
            n.st[b] = l->st[a];
            n.age[b] = l->age[a];
            n.tint[b] = l->tint[a];
        }
    life_free(l);
    *l = n;
}

void life_clear(Life *l)
{
    size_t n = (size_t)l->sw * (size_t)l->sh;
    memset(l->st, 0, n);
    memset(l->age, 0, n * sizeof *l->age);
    memset(l->tint, 0, n);
}

void life_set(Life *l, int x, int y, uint8_t state, uint8_t tint, uint16_t age)
{
    if (!in_sim(l, x, y))
        return;
    size_t i = idx(l, x, y);
    l->st[i] = state;
    l->tint[i] = tint < LIFE_TINTS ? tint : 0;
    l->age[i] = age;
}

uint8_t life_get(const Life *l, int x, int y)
{
    return in_sim(l, x, y) ? l->st[idx(l, x, y)] : CELL_OFF;
}

uint16_t life_age(const Life *l, int x, int y)
{
    return in_sim(l, x, y) ? l->age[idx(l, x, y)] : 0;
}

uint8_t life_tint(const Life *l, int x, int y)
{
    return in_sim(l, x, y) ? l->tint[idx(l, x, y)] : 0;
}

void life_set_rule(Life *l, Rule r)
{
    if (l->rule == RULE_BRAIN && r != RULE_BRAIN) {
        size_t n = (size_t)l->sw * (size_t)l->sh;
        for (size_t i = 0; i < n; i++)
            if (l->st[i] == CELL_DYING)
                l->st[i] = CELL_OFF;
    }
    l->rule = r;
}

/* Newborns take the most common tint among their live neighbors (ties: lowest tint). */
static uint8_t majority_tint(const Life *l, long i)
{
    const long sw = l->sw;
    const long off[8] = {-sw - 1, -sw, -sw + 1, -1, 1, sw - 1, sw, sw + 1};
    int count[LIFE_TINTS] = {0};
    for (int k = 0; k < 8; k++)
        if (l->st[i + off[k]] == CELL_ON)
            count[l->tint[i + off[k]]]++;
    int best = 0;
    for (int t = 1; t < LIFE_TINTS; t++)
        if (count[t] > count[best])
            best = t;
    return (uint8_t)best;
}

static uint16_t older(uint16_t a)
{
    return a < UINT16_MAX ? (uint16_t)(a + 1) : a;
}

void life_step(Life *l)
{
    const long sw = l->sw, sh = l->sh;
    const int brain = l->rule == RULE_BRAIN, highlife = l->rule == RULE_HIGHLIFE;
    memset(l->st2, 0, (size_t)(sw * sh));
    for (long y = 1; y < sh - 1; y++) {
        for (long x = 1; x < sw - 1; x++) {
            long i = y * sw + x;
            const uint8_t *s = l->st;
            int n = (s[i - sw - 1] == CELL_ON) + (s[i - sw] == CELL_ON) + (s[i - sw + 1] == CELL_ON) +
                    (s[i - 1] == CELL_ON) + (s[i + 1] == CELL_ON) + (s[i + sw - 1] == CELL_ON) +
                    (s[i + sw] == CELL_ON) + (s[i + sw + 1] == CELL_ON);
            uint8_t me = s[i];
            if (brain) {
                if (me == CELL_ON) {
                    l->st2[i] = CELL_DYING;
                    l->age2[i] = older(l->age[i]);
                    l->tint2[i] = l->tint[i];
                } else if (me == CELL_OFF && n == 2) {
                    l->st2[i] = CELL_ON;
                    l->age2[i] = 0;
                    l->tint2[i] = majority_tint(l, i);
                }
            } else if (me == CELL_ON) {
                if (n == 2 || n == 3) {
                    l->st2[i] = CELL_ON;
                    l->age2[i] = older(l->age[i]);
                    l->tint2[i] = l->tint[i];
                }
            } else if (n == 3 || (highlife && n == 6)) {
                l->st2[i] = CELL_ON;
                l->age2[i] = 0;
                l->tint2[i] = majority_tint(l, i);
            }
        }
    }
    for (long y = 0; y < sh; y++)
        for (long x = 0; x < sw; x++)
            if (x < ABSORB || y < ABSORB || x >= sw - ABSORB || y >= sh - ABSORB)
                l->st2[y * sw + x] = CELL_OFF;

    uint8_t *t8 = l->st;
    l->st = l->st2;
    l->st2 = t8;
    uint16_t *t16 = l->age;
    l->age = l->age2;
    l->age2 = t16;
    t8 = l->tint;
    l->tint = l->tint2;
    l->tint2 = t8;
}

int life_population(const Life *l)
{
    int n = 0;
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++)
            n += l->st[idx(l, x, y)] != CELL_OFF;
    return n;
}
