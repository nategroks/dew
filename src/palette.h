#ifndef DEW_PALETTE_H
#define DEW_PALETTE_H

#include "garden.h"
#include "life.h"

#include <stdint.h>

/* Every color dew draws, as 0xRRGGBB, computed from the 16 Nord values. */

#define PAL_AGES 8

typedef enum {
    UI_TEXT,   /* task titles */
    UI_DIM,    /* done tasks, hints, separators */
    UI_LABEL,  /* box labels, key names */
    UI_SEL,    /* selected row (fg) */
    UI_ACTIVE, /* the task the wave is on, the status line */
    UI_NOTE,   /* note text */
    UI_COUNT,  /* wave count */
    UI_BORDER, /* box lines */
    UI_WARN,   /* errors */
    UI_N
} UiColor;

typedef struct {
    uint32_t life[LIFE_TINTS][PAL_AGES];
    uint32_t gray[LIFE_TINTS][PAL_AGES]; /* paused */
    uint32_t garden[GARDEN_PAIRS][PAL_AGES]; /* plant gradients, 8 steps each */
    uint32_t dying;
    uint32_t bg, band, sel_bg;
    uint32_t foam[4];
    uint32_t river[4]; /* deep, mid, light water, bank */
    uint32_t wolf[4];  /* indexed by PX_SNOW-1 .. PX_DARK-1: snow, shade, eye, dark */
    uint32_t glyph[5];
    uint32_t ui[UI_N];
} PaletteRGB;

void palette_compute(PaletteRGB *p, const uint32_t nord[16]);
int palette_age_bucket(uint16_t age); /* 0 .. PAL_AGES-1 */
uint32_t rgb_mix(uint32_t a, uint32_t b, double f);
int palette_nearest256(uint32_t rgb); /* xterm index 16..255 */
int palette_nearest8(uint32_t rgb);   /* COLOR_BLACK..COLOR_WHITE */

#endif
