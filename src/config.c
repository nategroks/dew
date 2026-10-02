#include "config.h"

#include <stdlib.h>
#include <string.h>

static const uint32_t NORD[16] = {
    0x2e3440, 0x3b4252, 0x434c5e, 0x4c566a, /* polar night */
    0xd8dee9, 0xe5e9f0, 0xeceff4,           /* snow storm */
    0x8fbcbb, 0x88c0d0, 0x81a1c1, 0x5e81ac, /* frost */
    0xbf616a, 0xd08770, 0xebcb8b, 0xa3be8c, 0xb48ead, /* aurora */
};

void config_defaults(Config *c)
{
    c->focus_min = 25;
    c->short_min = 5;
    c->long_min = 15;
    c->long_every = 4;
    c->sprite = SPRITE_WAVE;
    c->bell = true;
    c->notify = true;
    c->glitch = true;
    c->fps = 20;
    c->focus_rule = rule_conway();
    memcpy(c->nord, NORD, sizeof NORD);
}

const char *sprite_text(Sprite s)
{
    switch (s) {
    case SPRITE_ASCII:
        return "≈";
    case SPRITE_BOLT:
        return "⚡";
    default:
        return "🌊";
    }
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        *--e = '\0';
    return s;
}

/* A comment is a line starting with '#', or " #" followed by a space or the end. */
static void strip_comment(char *s)
{
    char *t = s;
    while (*t == ' ' || *t == '\t')
        t++;
    if (*t == '#') {
        *s = '\0';
        return;
    }
    for (char *p = s; *p; p++)
        if (*p == '#' && p > s && (p[-1] == ' ' || p[-1] == '\t') &&
            (p[1] == '\0' || p[1] == ' ' || p[1] == '\t')) {
            *p = '\0';
            return;
        }
}

typedef int (*LineFn)(Config *c, const char *key, const char *val, size_t lineno, Sbuf *warn);

static int each_line(Config *c, const char *text, Sbuf *warn, LineFn fn)
{
    int problems = 0;
    size_t lineno = 0;
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        char *line = xstrndup(p, n);
        p = nl ? nl + 1 : p + n;
        lineno++;
        strip_comment(line);
        char *s = trim(line);
        char *eq = strchr(s, '=');
        if (!*s) {
            /* blank or comment */
        } else if (!eq) {
            sb_printf(warn, "config: line %zu: expected key = value\n", lineno);
            problems++;
        } else {
            *eq = '\0';
            problems += fn(c, trim(s), trim(eq + 1), lineno, warn);
        }
        free(line);
    }
    return problems;
}

static int set_range(int *dst, const char *key, const char *val, int lo, int hi, size_t lineno,
                     Sbuf *warn)
{
    int v;
    if (!parse_int(val, &v) || v < lo || v > hi) {
        sb_printf(warn, "config: line %zu: %s must be %d-%d\n", lineno, key, lo, hi);
        return 1;
    }
    *dst = v;
    return 0;
}

static int set_bool(bool *dst, const char *key, const char *val, size_t lineno, Sbuf *warn)
{
    if (strcmp(val, "1") == 0 || strcmp(val, "yes") == 0 || strcmp(val, "true") == 0) {
        *dst = true;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "no") == 0 || strcmp(val, "false") == 0) {
        *dst = false;
        return 0;
    }
    sb_printf(warn, "config: line %zu: %s must be 0 or 1\n", lineno, key);
    return 1;
}

static int config_line(Config *c, const char *key, const char *val, size_t lineno, Sbuf *warn)
{
    if (strcmp(key, "focus") == 0)
        return set_range(&c->focus_min, key, val, 1, 180, lineno, warn);
    if (strcmp(key, "short_break") == 0)
        return set_range(&c->short_min, key, val, 1, 60, lineno, warn);
    if (strcmp(key, "long_break") == 0)
        return set_range(&c->long_min, key, val, 1, 120, lineno, warn);
    if (strcmp(key, "long_every") == 0)
        return set_range(&c->long_every, key, val, 1, 12, lineno, warn);
    if (strcmp(key, "fps") == 0)
        return set_range(&c->fps, key, val, 5, 60, lineno, warn);
    if (strcmp(key, "bell") == 0)
        return set_bool(&c->bell, key, val, lineno, warn);
    if (strcmp(key, "notify") == 0)
        return set_bool(&c->notify, key, val, lineno, warn);
    if (strcmp(key, "glitch") == 0)
        return set_bool(&c->glitch, key, val, lineno, warn);
    if (strcmp(key, "focus_rule") == 0) {
        Rule r;
        if (!rule_parse(val, &r)) {
            sb_printf(warn, "config: line %zu: focus_rule must be a preset like daynight or a rule like B36/S23\n",
                      lineno);
            return 1;
        }
        c->focus_rule = r;
        return 0;
    }
    if (strcmp(key, "sprite") == 0) {
        if (strcmp(val, "wave") == 0)
            c->sprite = SPRITE_WAVE;
        else if (strcmp(val, "ascii") == 0)
            c->sprite = SPRITE_ASCII;
        else if (strcmp(val, "bolt") == 0)
            c->sprite = SPRITE_BOLT;
        else {
            sb_printf(warn, "config: line %zu: sprite must be wave, ascii or bolt\n", lineno);
            return 1;
        }
        return 0;
    }
    sb_printf(warn, "config: line %zu: unknown key \"%s\"\n", lineno, key);
    return 1;
}

static int hexval(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static int colors_line(Config *c, const char *key, const char *val, size_t lineno, Sbuf *warn)
{
    int idx;
    if (strncmp(key, "nord", 4) != 0 || !parse_int(key + 4, &idx) || idx < 0 || idx > 15) {
        sb_printf(warn, "colors: line %zu: unknown key \"%s\" (use nord0-nord15)\n", lineno, key);
        return 1;
    }
    uint32_t rgb = 0;
    bool ok = val[0] == '#' && strlen(val) == 7;
    for (int i = 1; ok && i < 7; i++) {
        int h = hexval(val[i]);
        ok = h >= 0;
        rgb = rgb << 4 | (uint32_t)(h < 0 ? 0 : h);
    }
    if (!ok) {
        sb_printf(warn, "colors: line %zu: %s must look like #2e3440\n", lineno, key);
        return 1;
    }
    c->nord[idx] = rgb;
    return 0;
}

int config_parse(Config *c, const char *text, Sbuf *warn)
{
    return each_line(c, text, warn, config_line);
}

int config_parse_colors(Config *c, const char *text, Sbuf *warn)
{
    return each_line(c, text, warn, colors_line);
}
