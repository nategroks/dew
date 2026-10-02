#include "rules.h"
#include "test.h"

static void test_parse_presets(void)
{
    Rule r;
    CHECK(rule_parse("conway", &r));
    CHECK(rule_eq(r, rule_conway()));
    CHECK(rule_parse("B3/S23", &r));
    CHECK(rule_eq(r, rule_conway()));
    CHECK(rule_parse("b36/s23", &r));
    CHECK_STR(rule_name(r), "highlife");
    CHECK(rule_parse("B2/S/3", &r));
    CHECK_INT(r.states, 3);
    CHECK(rule_eq(r, rule_brain()));
    CHECK(rule_parse("B2/S345/C4", &r));
    CHECK_STR(rule_name(r), "star wars");
    CHECK(rule_parse(" daynight ", &r));
    CHECK_STR(rule_name(r), "day & night");
    CHECK(rule_parse("Seeds", &r));
    CHECK_INT(r.survive, 0);
}

static void test_format_round_trip(void)
{
    char buf[32];
    for (int i = 0; i < RULE_PRESET_COUNT; i++) {
        Rule back;
        rule_format(RULE_PRESETS[i].rule, buf, sizeof buf);
        CHECK(rule_parse(buf, &back));
        CHECK(rule_eq(back, RULE_PRESETS[i].rule));
        CHECK(rule_parse(RULE_PRESETS[i].key, &back));
        CHECK(rule_eq(back, RULE_PRESETS[i].rule));
        CHECK_STR(rule_name(back), RULE_PRESETS[i].name);
    }
    rule_format(rule_conway(), buf, sizeof buf);
    CHECK_STR(buf, "B3/S23");
    rule_format(rule_brain(), buf, sizeof buf);
    CHECK_STR(buf, "B2/S/3");
}

static void test_custom_and_bad(void)
{
    Rule r;
    CHECK(rule_parse("B35/S236", &r));
    CHECK_STR(rule_name(r), "custom");
    CHECK(rule_parse("B3/S23/2", &r)); /* /2 is plain Life */
    CHECK(rule_eq(r, rule_conway()));
    static const char *bad[] = {"", "B9/S23", "X3/S23", "B3/S23/1", "B3S23", "B3/S23/",
                                "B3/S23/C256", "B3/Sx", "maze!", "B3/S23/C4x"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECK(!rule_parse(bad[i], &r));
}

void suite_rules(void)
{
    test_parse_presets();
    test_format_round_trip();
    test_custom_and_bad();
}
