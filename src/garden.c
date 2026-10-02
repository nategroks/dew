#include "garden.h"

#include "patterns.h"

#include <math.h>
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
    int prev = g->n > 1 ? g->p[g->n - 2].tint : -1;
    int pair = rng_range(r, GARDEN_PAIRS - (prev >= 0));
    if (prev >= 0 && pair >= prev)
        pair++; /* any pair but the previous plant's */
    p->tint = (uint8_t)pair;
    return x >= 0;
}

size_t garden_layout(const Garden *g, int w, int h, GardenBox **out)
{
    GardenBox *boxes = xcalloc(g->n, sizeof *boxes);
    size_t n = 0;
    for (size_t i = 0; i < g->n; i++) {
        const Plant *p = &g->p[i];
        if (p->x < 0)
            continue;
        Pattern pt;
        if (!pattern_get(p->pattern, &pt))
            continue;
        GardenBox b = {p->x * w / GARDEN_W, p->y * h / GARDEN_H, pt.w, pt.h, p->tint,
                       (uint8_t)pattern_period(p->pattern)};
        if (b.x < 1 || b.y < 1 || b.x + b.w > w - 1 || b.y + b.h > h - 1)
            continue;
        bool ok = true;
        for (size_t k = 0; ok && k < n; k++)
            ok = b.x >= boxes[k].x + boxes[k].w + STAMP_PAD || boxes[k].x >= b.x + b.w + STAMP_PAD ||
                 b.y >= boxes[k].y + boxes[k].h + STAMP_PAD || boxes[k].y >= b.y + b.h + STAMP_PAD;
        if (ok)
            boxes[n++] = b;
    }
    *out = boxes;
    return n;
}

void garden_stamp(const Garden *g, Life *l)
{
    GardenBox *boxes;
    size_t n = garden_layout(g, l->w, l->h, &boxes);
    for (size_t i = 0; i < n; i++) {
        /* boxes keep plant order, so find the plant by position */
        for (size_t k = 0; k < g->n; k++) {
            const Plant *p = &g->p[k];
            Pattern pt;
            if (p->x < 0 || !pattern_get(p->pattern, &pt))
                continue;
            if (p->x * l->w / GARDEN_W == boxes[i].x && p->y * l->h / GARDEN_H == boxes[i].y) {
                pattern_stamp(l, &pt, boxes[i].x, boxes[i].y, p->tint, STAMP_AGE);
                break;
            }
        }
    }
    free(boxes);
}

int garden_shade(const GardenBox *b, int x, int y, long frame)
{
    int span = b->w + b->h - 2;
    double pos = span > 0 ? (double)((x - b->x) + (y - b->y)) / span : 0.5;
    pos = pos < 0 ? 0 : pos > 1 ? 1 : pos;
    double f;
    if (b->period > 1) {
        double phase = (double)((frame / 8) % b->period) / b->period;
        double tri = 1 - fabs(2 * phase - 1);
        f = pos * 0.5 + 0.5 * tri;
    } else {
        f = pos * 0.6 + 0.4 * (0.5 + 0.5 * sin(frame * 2 * M_PI / 160));
    }
    int s = (int)lround(f * 7);
    return s < 0 ? 0 : s > 7 ? 7 : s;
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
                pattern_get(name, &pt) && tint >= 0 && tint < GARDEN_PAIRS && x >= -1 &&
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
