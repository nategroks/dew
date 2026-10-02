#include "test.h"
#include "util.h"

#include <stdlib.h>
#include <time.h>

static void test_sbuf(void)
{
    Sbuf b;
    sb_init(&b);
    sb_puts(&b, "wave ");
    sb_printf(&b, "%d of %s", 3, "today");
    CHECK_STR(b.buf, "wave 3 of today");
    CHECK_INT(b.len, 15);
    char *s = sb_take(&b);
    CHECK_STR(s, "wave 3 of today");
    CHECK(b.buf == NULL);
    free(s);

    char *empty = sb_take(&b);
    CHECK_STR(empty, "");
    free(empty);
}

static void test_rng(void)
{
    Rng a, b;
    rng_seed(&a, 42);
    rng_seed(&b, 42);
    for (int i = 0; i < 100; i++)
        CHECK_INT(rng_next(&a), rng_next(&b));
    for (int i = 0; i < 1000; i++) {
        int v = rng_range(&a, 7);
        CHECK(v >= 0 && v < 7);
        double u = rng_unit(&a);
        CHECK(u >= 0.0 && u < 1.0);
    }
    CHECK_INT(rng_range(&a, 0), 0);
    CHECK_INT(rng_range(&a, 1), 0);
}

static void test_parse_int(void)
{
    int v = 0;
    CHECK(parse_int("25", &v));
    CHECK_INT(v, 25);
    CHECK(parse_int("-3", &v));
    CHECK_INT(v, -3);
    CHECK(!parse_int("", &v));
    CHECK(!parse_int("2x", &v));
    CHECK(!parse_int(" 2", &v));
    CHECK(!parse_int("-", &v));
    CHECK(!parse_int("99999999999", &v));
}

static void test_utf8(void)
{
    uint32_t cp;
    CHECK_INT(utf8_decode("a", &cp), 1);
    CHECK_INT(cp, 'a');
    CHECK_INT(utf8_decode("é", &cp), 2);
    CHECK_INT(cp, 0xE9);
    CHECK_INT(utf8_decode("🌊", &cp), 4);
    CHECK_INT(cp, 0x1F30A);
    CHECK_INT(utf8_decode("\xff", &cp), 1);
    CHECK_INT(cp, 0xFFFD);
    CHECK_INT(utf8_decode("\xe2\x82", &cp), 1); /* truncated sequence */
    CHECK_INT(cp, 0xFFFD);
    CHECK_INT(utf8_decode("", &cp), 0);

    char out[4];
    CHECK_INT(utf8_encode(0x1F30A, out), 4);
    CHECK(memcmp(out, "🌊", 4) == 0);
    CHECK_INT(utf8_encode(0x2248, out), 3); /* ≈ */
    CHECK(memcmp(out, "≈", 3) == 0);
}

static void test_clock(void)
{
#ifdef CLOCK_BOOTTIME
    CHECK_INT(clock_source(), CLOCK_BOOTTIME); /* keeps counting through suspend */
#endif
    double a = clock_now(), b = clock_now();
    CHECK(a > 0 && b >= a);
}

void suite_util(void)
{
    test_sbuf();
    test_rng();
    test_parse_int();
    test_utf8();
    test_clock();
}
