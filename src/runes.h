#ifndef DEW_RUNES_H
#define DEW_RUNES_H

#include "runeart.h"

#include <stdint.h>

/*
 * The 24 Elder Futhark runes that mark tasks, from tools/runes.py (the same
 * art the slice-rune window titlebars use). A rune is an index into RUNE_ART.
 */

#define RUNE_NONE (-1)

int rune_find(const char *name); /* a name or another spelling ("perth"), any case; RUNE_NONE if unknown */
const char *rune_name(int i);    /* "perthro"; "" when out of range */
uint32_t rune_rgb(const uint32_t nord[16], int i); /* its soft Nord color */
int rune_pixel(int i, int x, int y);               /* 1 where the art has ink */
/* Draws rune i into a w x h RGBA canvas (cleared to transparent), scaled up by the largest
   whole number that fits (at least 1) and centered. Returns the scale. */
int rune_canvas(const uint32_t nord[16], int i, int w, int h, uint8_t *rgba);

#endif
