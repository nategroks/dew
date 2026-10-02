#ifndef DEW_LIFE_H
#define DEW_LIFE_H

#include "rules.h"

#include <stdint.h>

/*
 * A cellular automaton board. Coordinates are visible-board cells,
 * (0,0) top-left. A hidden LIFE_MARGIN-cell border surrounds the board
 * so patterns can fly off the edge; its outermost 3 cells are cleared
 * every step, which absorbs anything that leaves.
 */

#define LIFE_MARGIN 8
#define LIFE_TINTS 6 /* tint 0 = age ramp, 1..5 = fleet / garden colors */

/* States 2 and up are "dying" (fading) cells of a Generations rule. */
enum { CELL_OFF = 0, CELL_ON = 1, CELL_DYING = 2 };

typedef struct {
    int w, h;   /* visible size */
    int sw, sh; /* simulated size, including margins */
    uint8_t *st, *st2;
    uint16_t *age, *age2;
    uint8_t *tint, *tint2;
    Rule rule;
} Life;

void life_init(Life *l, int w, int h); /* negative sizes count as 0 */
void life_free(Life *l);
void life_resize(Life *l, int w, int h); /* keeps the cells that still fit */
void life_clear(Life *l);
void life_set(Life *l, int x, int y, uint8_t state, uint8_t tint, uint16_t age); /* ignores out of range */
uint8_t life_get(const Life *l, int x, int y);   /* CELL_OFF outside the simulated area */
uint16_t life_age(const Life *l, int x, int y);
uint8_t life_tint(const Life *l, int x, int y);
void life_set_rule(Life *l, Rule r); /* turns off cells in states the new rule lacks */
void life_step(Life *l);
int life_population(const Life *l); /* live (on or dying) cells on the visible board */

#endif
