#ifndef DEW_GARDEN_H
#define DEW_GARDEN_H

#include "life.h"
#include "util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * One plant per finished wave. Positions live in a fixed GARDEN_W x
 * GARDEN_H space so the garden looks the same on every terminal size;
 * garden_stamp scales them onto whatever board is showing.
 */

#define GARDEN_W 120
#define GARDEN_H 64
#define GARDEN_PAD 8 /* room for oscillators to swing without touching */
#define GARDEN_PAIRS 9 /* color gradients a plant can wear */

typedef struct {
    char hhmm[6];
    char pattern[16];
    int x, y; /* -1, -1 when there was no room */
    uint8_t tint; /* color pair, 0 .. GARDEN_PAIRS-1 */
} Plant;

typedef struct Garden {
    Plant *p;
    size_t n, cap;
} Garden;

void garden_init(Garden *g);
void garden_free(Garden *g);

/* Pattern for the n-th wave of the day (n starts at 1). */
const char *garden_pattern_for(int n);

/* Records the next plant. Returns false if it had to be recorded without a spot. */
bool garden_plant(Garden *g, Rng *r, const char *hhmm);

/* Where each plant that fits lands on a w x h board. */
typedef struct {
    int x, y, w, h;
    uint8_t pair, period;
} GardenBox;

/* Fills *out (malloc'd, caller frees) and returns how many plants fit. */
size_t garden_layout(const Garden *g, int w, int h, GardenBox **out);

/* Stamps every plant that fits onto l (does not clear l). */
void garden_stamp(const Garden *g, Life *l);

/* Gradient step 0..7 for a cell at (x, y) of box b: diagonal across the plant,
   cycling once per oscillation for oscillators, breathing slowly for still lifes. */
int garden_shade(const GardenBox *b, int x, int y, long frame);

char *garden_serialize(const Garden *g);       /* "HH:MM pattern x y tint\n" per plant */
void garden_parse(Garden *g, const char *text); /* appends valid lines, skips the rest */

#endif
