#include "config.h"
#include "test.h"

#include <stdlib.h>

static void test_defaults(void)
{
    Config c;
    config_defaults(&c);
    CHECK_INT(c.focus_min, 25);
    CHECK_INT(c.short_min, 5);
    CHECK_INT(c.long_min, 15);
    CHECK_INT(c.long_every, 4);
    CHECK_INT(c.sprite, SPRITE_WAVE);
    CHECK(c.bell && c.notify && c.glitch);
    CHECK_INT(c.fps, 20);
    CHECK_INT(c.nord[0], 0x2e3440);
    CHECK_INT(c.nord[15], 0xb48ead);
    CHECK_STR(sprite_text(SPRITE_ASCII), "≈");
}

static void test_parse(void)
{
    Config c;
    config_defaults(&c);
    Sbuf w;
    sb_init(&w);
    int n = config_parse(&c,
                         "# dew config\n"
                         "focus = 50     # minutes\n"
                         "\n"
                         "short_break=10\n"
                         "  long_break = 20\r\n"
                         "long_every = 3\n"
                         "sprite = ascii\n"
                         "bell = 0\n"
                         "notify = no\n"
                         "glitch = false\n"
                         "fps = 30\n",
                         &w);
    CHECK_INT(n, 0);
    CHECK(w.len == 0);
    CHECK_INT(c.focus_min, 50);
    CHECK_INT(c.short_min, 10);
    CHECK_INT(c.long_min, 20);
    CHECK_INT(c.long_every, 3);
    CHECK_INT(c.sprite, SPRITE_ASCII);
    CHECK(!c.bell && !c.notify && !c.glitch);
    CHECK_INT(c.fps, 30);
    sb_free(&w);
}

static void test_bad_values(void)
{
    Config c;
    config_defaults(&c);
    Sbuf w;
    sb_init(&w);
    int n = config_parse(&c,
                         "focus = 0\n"
                         "fps = fast\n"
                         "colour = blue\n"
                         "sprite = fish\n"
                         "bell = maybe\n"
                         "just words\n",
                         &w);
    CHECK_INT(n, 6);
    CHECK_INT(c.focus_min, 25); /* unchanged */
    CHECK_INT(c.fps, 20);
    CHECK(strstr(w.buf, "line 1: focus must be 1-180") != NULL);
    CHECK(strstr(w.buf, "line 3: unknown key \"colour\"") != NULL);
    CHECK(strstr(w.buf, "line 6: expected key = value") != NULL);
    sb_free(&w);
}

static void test_colors(void)
{
    Config c;
    config_defaults(&c);
    Sbuf w;
    sb_init(&w);
    int n = config_parse_colors(&c,
                                "# my nord\n"
                                "nord0 = #000000\n"
                                "nord8 = #A0B0C0  # frost\n"
                                "nord16 = #ffffff\n"
                                "nord1 = 2e3440\n",
                                &w);
    CHECK_INT(n, 2);
    CHECK_INT(c.nord[0], 0x000000);
    CHECK_INT(c.nord[8], 0xa0b0c0);
    CHECK_INT(c.nord[1], 0x3b4252); /* unchanged */
    sb_free(&w);
}

void suite_config(void)
{
    test_defaults();
    test_parse();
    test_bad_values();
    test_colors();
}
