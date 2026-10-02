#ifndef DEW_CONFIG_H
#define DEW_CONFIG_H

#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum { SPRITE_WAVE, SPRITE_ASCII, SPRITE_BOLT } Sprite;

typedef struct {
    int focus_min, short_min, long_min, long_every;
    Sprite sprite;
    bool bell, notify, glitch;
    int fps;
    uint32_t nord[16]; /* 0xRRGGBB, nord0..nord15 */
} Config;

void config_defaults(Config *c);

/*
 * Parse "key = value" lines into c. Bad lines leave the current value,
 * append one "config: line N: ..." message per problem to warn, and are
 * counted in the return value.
 */
int config_parse(Config *c, const char *text, Sbuf *warn);

/* Same, for the colors file: "nord0 = #2e3440" ... "nord15 = #b48ead". */
int config_parse_colors(Config *c, const char *text, Sbuf *warn);

const char *sprite_text(Sprite s); /* "🌊", "≈", "⚡" */

#endif
