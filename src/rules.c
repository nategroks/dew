#include "rules.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

const RulePreset RULE_PRESETS[] = {
    {"conway", "conway", {1u << 3, (1u << 2) | (1u << 3), 2}},
    {"highlife", "highlife", {(1u << 3) | (1u << 6), (1u << 2) | (1u << 3), 2}},
    {"daynight", "day & night",
     {(1u << 3) | (1u << 6) | (1u << 7) | (1u << 8),
      (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 8), 2}},
    {"seeds", "seeds", {1u << 2, 0, 2}},
    {"lifewithoutdeath", "life without death", {1u << 3, 0x1ff, 2}},
    {"maze", "maze", {1u << 3, (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5), 2}},
    {"2x2", "2x2", {(1u << 3) | (1u << 6), (1u << 1) | (1u << 2) | (1u << 5), 2}},
    {"morley", "morley", {(1u << 3) | (1u << 6) | (1u << 8), (1u << 2) | (1u << 4) | (1u << 5), 2}},
    {"brain", "brian's brain", {1u << 2, 0, 3}},
    {"starwars", "star wars", {1u << 2, (1u << 3) | (1u << 4) | (1u << 5), 4}},
};
const int RULE_PRESET_COUNT = (int)(sizeof RULE_PRESETS / sizeof *RULE_PRESETS);

Rule rule_conway(void) { return RULE_PRESETS[0].rule; }
Rule rule_highlife(void) { return RULE_PRESETS[1].rule; }
Rule rule_brain(void) { return RULE_PRESETS[8].rule; }

bool rule_eq(Rule a, Rule b)
{
    return a.born == b.born && a.survive == b.survive && a.states == b.states;
}

/* Parses "<letter><digits>" up to the next '/' or end; digits 0-8. */
static const char *parse_digits(const char *p, char letter, uint16_t *bits)
{
    if (tolower((unsigned char)*p) != letter)
        return NULL;
    p++;
    *bits = 0;
    for (; *p && *p != '/'; p++) {
        if (*p < '0' || *p > '8')
            return NULL;
        *bits |= (uint16_t)(1u << (*p - '0'));
    }
    return p;
}

bool rule_parse(const char *s, Rule *out)
{
    char buf[48];
    while (isspace((unsigned char)*s))
        s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    if (n == 0 || n >= sizeof buf)
        return false;
    memcpy(buf, s, n);
    buf[n] = '\0';

    for (int i = 0; i < RULE_PRESET_COUNT; i++)
        if (strcasecmp(buf, RULE_PRESETS[i].key) == 0) {
            *out = RULE_PRESETS[i].rule;
            return true;
        }

    Rule r = {0, 0, 2};
    const char *p = parse_digits(buf, 'b', &r.born);
    if (!p || *p != '/')
        return false;
    p = parse_digits(p + 1, 's', &r.survive);
    if (!p)
        return false;
    if (*p == '/') {
        p++;
        if (tolower((unsigned char)*p) == 'c')
            p++;
        if (!isdigit((unsigned char)*p))
            return false;
        int states = 0;
        for (; isdigit((unsigned char)*p); p++) {
            states = states * 10 + (*p - '0');
            if (states > 255)
                return false;
        }
        if (*p || states < 2)
            return false;
        r.states = (uint8_t)states;
    } else if (*p) {
        return false;
    }
    *out = r;
    return true;
}

void rule_format(Rule r, char *out, size_t n)
{
    char b[10] = "", s[10] = "";
    for (int i = 0, kb = 0, ks = 0; i <= 8; i++) {
        if (r.born & (1u << i))
            b[kb++] = (char)('0' + i);
        if (r.survive & (1u << i))
            s[ks++] = (char)('0' + i);
    }
    if (r.states > 2)
        snprintf(out, n, "B%s/S%s/%u", b, s, (unsigned)r.states);
    else
        snprintf(out, n, "B%s/S%s", b, s);
}

const char *rule_name(Rule r)
{
    for (int i = 0; i < RULE_PRESET_COUNT; i++)
        if (rule_eq(r, RULE_PRESETS[i].rule))
            return RULE_PRESETS[i].name;
    return "custom";
}
