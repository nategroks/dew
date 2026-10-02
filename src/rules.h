#ifndef DEW_RULES_H
#define DEW_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A Life-like or "Generations" cellular automaton rule. Bit n of born /
 * survive means "n live neighbours". states == 2 is ordinary Life; with
 * more states a cell that fails to survive fades through states 2 ..
 * states-1 before turning off (Brian's Brain is B2/S/3).
 */
typedef struct {
    uint16_t born, survive;
    uint8_t states;
} Rule;

typedef struct {
    const char *key;  /* config name, e.g. "daynight" */
    const char *name; /* label, e.g. "day & night" */
    Rule rule;
} RulePreset;

extern const RulePreset RULE_PRESETS[];
extern const int RULE_PRESET_COUNT;

Rule rule_conway(void);
Rule rule_highlife(void);
Rule rule_brain(void);

bool rule_eq(Rule a, Rule b);
/* A preset key ("daynight") or a rule string ("B3678/S34678", "B2/S345/4", "B2/S345/C4"). */
bool rule_parse(const char *s, Rule *out);
void rule_format(Rule r, char *out, size_t n); /* "B3/S23", "B2/S/3" */
const char *rule_name(Rule r);                 /* preset name, or "custom" */

#endif
