#include "garden.h"

#include "patterns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAMP_AGE 6
#define STAMP_PAD 4

void garden_init(Garden *g)
{
    g->p = NULL;
    g->n = g->cap = 0;
}

void garden_free(Garden *g)
{
    free(g->p);
    garden_init(g);
}

static Plant *push(Garden *g)
{
    if (g->n == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 16;
        g->p = xrealloc(g->p, g->cap * sizeof *g->p);
    }
    Plant *p = &g->p[g->n++];
    memset(p, 0, sizeof *p);
    return p;
}

const char *garden_pattern_for(int n)
{
    static const char *ORDER[] = {"beehive", "blinker", "loaf", "toad",
                                  "boat",    "beacon",  "tub",  "block"};
    if (n < 1)
        n = 1;
    if (n % 8 == 0)
        return "pulsar";
    if (n % 4 == 0)
        return "penta";
    return ORDER[(n - 1 - n / 4) % 8];
}

typedef struct {
    int x, y, w, h;
} Box;

static bool clash(Box a, Box b, int pad)
{
    return a.x < b.x + b.w + pad && b.x < a.x + a.w + pad && a.y < b.y + b.h + pad &&
           b.y < a.y + a.h + pad;
}

static Box plant_box(const Plant *p)
{
    Pattern pt;
    pattern_get(p->pattern, &pt);
    return (Box){p->x, p->y, pt.w, pt.h};
}

bool garden_plant(Garden *g, Rng *r, const char *hhmm)
{
    int n = (int)g->n + 1;
    const char *name = garden_pattern_for(n);
    Pattern pt;
    pattern_get(name, &pt);

    int x = -1, y = -1;
    for (int tries = 0; tries < 200 && x < 0; tries++) {
        Box b = {3 + rng_range(r, GARDEN_W - pt.w - 6), 3 + rng_range(r, GARDEN_H - pt.h - 6),
                 pt.w, pt.h};
        bool ok = true;
        for (size_t i = 0; ok && i < g->n; i++)
            if (g->p[i].x >= 0 && clash(b, plant_box(&g->p[i]), GARDEN_PAD))
                ok = false;
        if (ok) {
            x = b.x;
            y = b.y;
        }
    }

    Plant *p = push(g);
    snprintf(p->hhmm, sizeof p->hhmm, "%.5s", hhmm);
    snprintf(p->pattern, sizeof p->pattern, "%s", name);
    p->x = x;
    p->y = y;
    p->tint = (uint8_t)(1 + n % 5);
    return x >= 0;
}

void garden_stamp(const Garden *g, Life *l)
{
    Box *placed = xcalloc(g->n, sizeof *placed);
    size_t np = 0;
    for (size_t i = 0; i < g->n; i++) {
        const Plant *p = &g->p[i];
        if (p->x < 0)
            continue;
        Pattern pt;
        if (!pattern_get(p->pattern, &pt))
            continue;
        Box b = {p->x * l->w / GARDEN_W, p->y * l->h / GARDEN_H, pt.w, pt.h};
        if (b.x < 1 || b.y < 1 || b.x + b.w > l->w - 1 || b.y + b.h > l->h - 1)
            continue;
        bool ok = true;
        for (size_t k = 0; ok && k < np; k++)
            ok = !clash(b, placed[k], STAMP_PAD);
        if (!ok)
            continue;
        pattern_stamp(l, &pt, b.x, b.y, p->tint, STAMP_AGE);
        placed[np++] = b;
    }
    free(placed);
}

char *garden_serialize(const Garden *g)
{
    Sbuf b;
    sb_init(&b);
    for (size_t i = 0; i < g->n; i++)
        sb_printf(&b, "%s %s %d %d %u\n", g->p[i].hhmm, g->p[i].pattern, g->p[i].x, g->p[i].y,
                  (unsigned)g->p[i].tint);
    return sb_take(&b);
}

void garden_parse(Garden *g, const char *text)
{
    const char *s = text;
    while (*s) {
        const char *nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        char line[128];
        if (n < sizeof line) {
            memcpy(line, s, n);
            line[n] = '\0';
            char hhmm[6], name[16];
            int x, y, tint;
            Pattern pt;
            if (sscanf(line, "%5s %15s %d %d %d", hhmm, name, &x, &y, &tint) == 5 &&
                pattern_get(name, &pt) && tint >= 0 && tint < LIFE_TINTS && x >= -1 &&
                x < GARDEN_W && y >= -1 && y < GARDEN_H) {
                Plant *p = push(g);
                memcpy(p->hhmm, hhmm, sizeof hhmm);
                memcpy(p->pattern, name, sizeof name);
                p->x = x;
                p->y = y;
                p->tint = (uint8_t)tint;
            }
        }
        s = nl ? nl + 1 : s + n;
    }
}
