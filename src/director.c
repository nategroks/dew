#include "director.h"

#include "patterns.h"

#include <math.h>
#include <string.h>

#define SWEEP_DELAY 40     /* frames between the burst and the break sweep */
#define SWEEP_SPEED 3.0    /* board cells per frame */
#define EXCITE_FRAMES 150  /* idle fast-forward after a fleet */
#define HIGHLIFE_FRAMES 180

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

void director_init(Director *d, int w, int h, uint64_t seed, const Garden *g)
{
    memset(d, 0, sizeof *d);
    life_init(&d->life, w, h);
    rng_seed(&d->rng, seed);
    d->garden = g;
    d->mood = MOOD_IDLE;
    d->focus_rule = rule_conway();
    director_show_garden(d);
}

void director_free(Director *d)
{
    life_free(&d->life);
}

void director_show_garden(Director *d)
{
    life_clear(&d->life);
    life_set_rule(&d->life, rule_conway());
    if (d->garden)
        garden_stamp(d->garden, &d->life);
}

void director_resize(Director *d, int w, int h)
{
    life_resize(&d->life, w, h);
    if (d->mood == MOOD_IDLE && !d->sweeping)
        director_show_garden(d);
}

static uint8_t next_tint(Director *d)
{
    d->tint_cursor = (uint8_t)(d->tint_cursor % 5 + 1);
    return d->tint_cursor;
}

static void soup(Director *d, int x0, int y0, int w, int h, double density, uint8_t tint)
{
    for (int y = imax(0, y0); y < imin(d->life.h, y0 + h); y++)
        for (int x = imax(0, x0); x < imin(d->life.w, x0 + w); x++)
            if (rng_unit(&d->rng) < density)
                life_set(&d->life, x, y, CELL_ON, tint, 0);
}

static int keepalive(const Life *l)
{
    return imax(8, l->w * l->h / 100);
}

static void reset_effects(Director *d)
{
    d->sweeping = false;
    d->pending_sweep = 0;
    d->excite = 0;
    d->highlife_left = 0;
}

void director_on_wave_start(Director *d)
{
    Life *l = &d->life;
    reset_effects(d);
    d->mood = MOOD_FOCUS;
    d->tear = 4;
    life_clear(l);
    life_set_rule(l, d->focus_rule);
    int band = imin(20, imax(4, l->h / 2));
    int margin = l->w / 16;
    soup(d, margin, rng_range(&d->rng, imax(1, l->h - band)), l->w - 2 * margin, band, 0.34, 0);
}

void director_set_focus_rule(Director *d, Rule r)
{
    d->focus_rule = r;
    if ((d->mood == MOOD_FOCUS || d->mood == MOOD_PAUSED) && !d->sweeping) {
        d->highlife_left = 0;
        life_set_rule(&d->life, r);
        d->tear = 3;
    }
}

void director_on_pause(Director *d)
{
    if (d->mood == MOOD_FOCUS)
        d->mood = MOOD_PAUSED;
}

void director_on_resume(Director *d)
{
    if (d->mood != MOOD_PAUSED)
        return;
    d->mood = MOOD_FOCUS;
    Life *l = &d->life;
    if (life_population(l) < keepalive(l))
        soup(d, l->w / 6, l->h / 4, l->w * 2 / 3, l->h / 2, 0.3, 0);
}

static void fleet(Director *d)
{
    Life *l = &d->life;
    uint8_t t = next_tint(d);
    Pattern p;
    if (rng_unit(&d->rng) < 0.55) {
        bool left = rng_range(&d->rng, 2), up = rng_range(&d->rng, 2);
        pattern_get("glider", &p);
        pattern_flip(&p, left, up);
        int count = imax(1, imin(5, (imin(l->w, l->h) - 4) / 7));
        for (int k = 0; k < count; k++) {
            int x = 2 + k * 7, y = 2 + (count - 1 - k) * 7;
            if (left)
                x = l->w - 3 - x;
            if (up)
                y = l->h - 3 - y;
            pattern_stamp(l, &p, x, y, t, 0);
        }
    } else {
        bool right = rng_range(&d->rng, 2);
        pattern_get("lwss", &p);
        pattern_flip(&p, right, false);
        int count = imax(1, imin(4, l->h / 15));
        int spacing = imax(1, l->h / count);
        for (int k = 0; k < count; k++) {
            int y = spacing * k + (spacing - 4) / 2;
            int off = (2 * k - (count - 1)) * 2;
            off = off < 0 ? -off : off;
            pattern_stamp(l, &p, right ? 1 + off : l->w - 6 - off, y, t, 0);
        }
    }
}

void director_on_task_done(Director *d)
{
    fleet(d);
    if (d->mood == MOOD_IDLE && !d->sweeping)
        d->excite = EXCITE_FRAMES;
}

static void burst(Director *d)
{
    static const char *NAMES[] = {"rpent", "acorn", "diehard"};
    Life *l = &d->life;
    Pattern p;
    pattern_get(NAMES[rng_range(&d->rng, 3)], &p);
    pattern_flip(&p, rng_range(&d->rng, 2), rng_range(&d->rng, 2));
    int x = l->w / 3 + rng_range(&d->rng, imax(1, l->w / 3)) - p.w / 2;
    int y = l->h / 3 + rng_range(&d->rng, imax(1, l->h / 3)) - p.h / 2;
    pattern_stamp(l, &p, x, y, 0, 0);
}

void director_on_wave_finish(Director *d)
{
    reset_effects(d);
    d->mood = MOOD_BREAK;
    life_set_rule(&d->life, rule_conway());
    burst(d);
    d->tear = 6;
    d->pending_sweep = SWEEP_DELAY;
}

static void start_sweep(Director *d, SweepThen then)
{
    d->sweeping = true;
    d->sweep_x = -12;
    d->then = then;
}

void director_on_break_end(Director *d)
{
    reset_effects(d);
    d->mood = MOOD_IDLE;
    start_sweep(d, THEN_GARDEN);
}

void director_on_abandon(Director *d)
{
    reset_effects(d);
    d->mood = MOOD_IDLE;
    d->tear = 3;
    director_show_garden(d);
}

double director_front(const Director *d, int y)
{
    return d->sweep_x + 5 * sin(y * 0.2 + d->frame * 0.3) + 3 * sin(y * 0.07 - d->frame * 0.1);
}

static void brain_seed(Director *d)
{
    Life *l = &d->life;
    int blobs = imax(2, imin(7, l->w * l->h / 1100));
    for (int k = 0; k < blobs; k++) {
        int cx = rng_range(&d->rng, imax(1, l->w)), cy = rng_range(&d->rng, imax(1, l->h));
        uint8_t t = (uint8_t)(1 + rng_range(&d->rng, 5));
        for (int y = -4; y <= 4; y++)
            for (int x = -4; x <= 4; x++)
                if (x * x + y * y <= 16 && rng_unit(&d->rng) < 0.45 && cx + x >= 0 &&
                    cx + x < l->w && cy + y >= 0 && cy + y < l->h)
                    life_set(l, cx + x, cy + y, CELL_ON, t, 0);
    }
}

static void advance_sweep(Director *d)
{
    Life *l = &d->life;
    d->sweep_x += SWEEP_SPEED;
    for (int y = 0; y < l->h; y++) {
        int lim = (int)floor(director_front(d, y) - 1);
        if (lim > l->w)
            lim = l->w;
        for (int x = 0; x < lim; x++)
            life_set(l, x, y, CELL_OFF, 0, 0);
    }
    if (d->sweep_x <= l->w + 14)
        return;
    d->sweeping = false;
    if (d->then == THEN_BRAIN) {
        life_clear(l);
        life_set_rule(l, rule_brain());
        brain_seed(d);
    } else {
        director_show_garden(d);
    }
}

static void decay(Director *d)
{
    Life *l = &d->life;
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++)
            if (life_get(l, x, y) && rng_unit(&d->rng) < 0.03)
                life_set(l, x, y, CELL_OFF, 0, 0);
}

void director_frame(Director *d)
{
    Life *l = &d->life;
    d->frame++;
    if (d->tear > 0)
        d->tear--;
    if (d->pending_sweep > 0 && --d->pending_sweep == 0)
        start_sweep(d, THEN_BRAIN);
    if (d->sweeping) {
        advance_sweep(d);
        return;
    }
    switch (d->mood) {
    case MOOD_FOCUS:
        if (d->frame % 2 == 0)
            life_step(l);
        if (d->frame % 60 == 0 && life_population(l) < keepalive(l))
            soup(d, rng_range(&d->rng, imax(1, l->w - 24)), rng_range(&d->rng, imax(1, l->h - 20)),
                 24, 20, 0.3, 0);
        if (d->highlife_left > 0 && --d->highlife_left == 0)
            life_set_rule(l, d->focus_rule);
        if (d->frame % 400 == 0 && rule_eq(d->focus_rule, rule_conway()) &&
            rule_eq(l->rule, rule_conway()) && rng_unit(&d->rng) < 0.45) {
            life_set_rule(l, rule_highlife());
            d->highlife_left = HIGHLIFE_FRAMES;
        }
        break;
    case MOOD_PAUSED:
        if (d->frame % 5 == 0)
            life_step(l);
        decay(d);
        break;
    case MOOD_BREAK:
        life_step(l);
        break;
    case MOOD_IDLE:
        if (d->excite > 0) {
            life_step(l);
            life_step(l);
            if (--d->excite == 0) {
                d->tear = 3;
                director_show_garden(d);
            }
        } else if (d->frame % 8 == 0) {
            life_step(l);
        }
        break;
    }
}

bool director_animating(const Director *d)
{
    return d->mood != MOOD_IDLE || d->sweeping || d->tear > 0 || d->excite > 0 ||
           d->pending_sweep > 0;
}
