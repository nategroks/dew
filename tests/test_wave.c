#include "test.h"
#include "wave.h"

static const WaveCfg CFG = {.focus_s = 1500, .short_s = 300, .long_s = 900, .long_every = 4};

static void test_happy_path(void)
{
    Wave w;
    wave_init(&w, CFG);
    CHECK_INT(w.mode, WAVE_IDLE);
    CHECK(wave_start(&w, 100));
    CHECK_INT(w.mode, WAVE_FOCUS);
    CHECK_INT(wave_remaining(&w, 100), 1500);
    CHECK_INT(wave_tick(&w, 1599), WEV_NONE);
    CHECK_INT(wave_tick(&w, 1600), WEV_FOCUS_DONE);
    CHECK_INT(w.mode, WAVE_BREAK);
    CHECK_INT(w.waves_today, 1);
    CHECK(!w.long_break);
    CHECK_INT(wave_remaining(&w, 1600), 300);
    CHECK_INT(wave_tick(&w, 1900), WEV_BREAK_DONE);
    CHECK_INT(w.mode, WAVE_IDLE);
    CHECK_INT(wave_tick(&w, 5000), WEV_NONE);
}

static void test_pause_resume(void)
{
    Wave w;
    wave_init(&w, CFG);
    wave_start(&w, 0);
    CHECK(wave_pause(&w, 600));
    CHECK_INT(wave_remaining(&w, 99999), 900); /* frozen while paused */
    CHECK_INT(wave_tick(&w, 99999), WEV_NONE);
    CHECK(wave_resume(&w, 10000));
    CHECK_INT(wave_remaining(&w, 10000), 900);
    CHECK_INT(wave_tick(&w, 10900), WEV_FOCUS_DONE);
}

static void test_long_break_cadence(void)
{
    Wave w;
    wave_init(&w, CFG);
    double t = 0;
    for (int i = 1; i <= 8; i++) {
        wave_start(&w, t);
        t += 1500;
        CHECK_INT(wave_tick(&w, t), WEV_FOCUS_DONE);
        CHECK_INT(w.long_break, i % 4 == 0);
        t += w.long_break ? 900 : 300;
        CHECK_INT(wave_tick(&w, t), WEV_BREAK_DONE);
    }
    CHECK_INT(w.waves_today, 8);
}

static void test_manual_transitions(void)
{
    Wave w;
    wave_init(&w, CFG);
    CHECK(!wave_pause(&w, 0));
    CHECK(!wave_resume(&w, 0));
    CHECK(!wave_finish(&w, 0));
    CHECK(!wave_abandon(&w));
    CHECK(!wave_end_break(&w));

    wave_start(&w, 0);
    CHECK(!wave_start(&w, 1)); /* already focusing */
    CHECK(wave_finish(&w, 60)); /* early finish counts */
    CHECK_INT(w.waves_today, 1);
    CHECK_INT(wave_remaining(&w, 60), 300);
    CHECK(wave_start(&w, 100)); /* skip the break */
    CHECK_INT(w.mode, WAVE_FOCUS);
    CHECK(wave_abandon(&w));
    CHECK_INT(w.mode, WAVE_IDLE);
    CHECK_INT(w.waves_today, 1); /* abandon does not count */

    wave_start(&w, 0);
    wave_pause(&w, 10);
    CHECK(wave_finish(&w, 20)); /* finishing while paused works */
    CHECK(wave_end_break(&w));
    CHECK_INT(w.mode, WAVE_IDLE);
}

static void test_restore_late(void)
{
    /* dew quit with 100 s of focus left and comes back 1000 s later */
    Wave a;
    wave_init(&a, CFG);
    wave_start(&a, 0);
    WaveSnap s = wave_snapshot(&a, 1400);
    CHECK_INT(s.mode, WAVE_FOCUS);
    CHECK_INT(s.remaining, 100);

    Wave b;
    wave_init(&b, CFG);
    s.remaining -= 1000; /* what the app computes from the saved wall-clock end */
    wave_restore(&b, s, 50);
    CHECK_INT(wave_tick(&b, 50), WEV_FOCUS_DONE);
    CHECK_INT(b.mode, WAVE_BREAK);
    /* the break started when focus ended (900 s ago), so 300 s of break are over too */
    CHECK_INT(wave_tick(&b, 50), WEV_BREAK_DONE);
    CHECK_INT(wave_tick(&b, 50), WEV_NONE);
    CHECK_INT(b.waves_today, 1);

    /* paused waves restore exactly */
    wave_init(&a, CFG);
    wave_start(&a, 0);
    wave_pause(&a, 500);
    s = wave_snapshot(&a, 9999);
    wave_init(&b, CFG);
    wave_restore(&b, s, 0);
    CHECK_INT(b.mode, WAVE_PAUSED);
    CHECK_INT(wave_remaining(&b, 0), 1000);
}

static void test_scaled_breaks(void)
{
    WaveCfg c = wave_scaled(CFG, 15 * 60); /* 25/5/15 scaled to 15: 3 and 9 minute breaks */
    CHECK_INT(c.focus_s, 900);
    CHECK_INT(c.short_s, 180);
    CHECK_INT(c.long_s, 540);
    CHECK_INT(c.long_every, 4);
    c = wave_scaled(CFG, 45 * 60);
    CHECK_INT(c.focus_s, 2700);
    CHECK_INT(c.short_s, 540);
    CHECK_INT(c.long_s, 1620);
    c = wave_scaled(CFG, 25 * 60);
    CHECK_INT(c.short_s, 300);
    CHECK_INT(c.long_s, 900);
    WaveCfg tiny = {.focus_s = 3000, .short_s = 60, .long_s = 120, .long_every = 2};
    c = wave_scaled(tiny, 15 * 60); /* whole minutes, never under one */
    CHECK_INT(c.short_s, 60);
    CHECK_INT(c.long_s, 60);

    int l[4];
    CHECK_INT(wave_lengths(25, l), 3);
    CHECK_INT(l[0], 15);
    CHECK_INT(l[1], 25);
    CHECK_INT(l[2], 45);
    CHECK_INT(wave_lengths(30, l), 4); /* the config's own length joins them, in order */
    CHECK_INT(l[2], 30);
    CHECK_INT(l[3], 45);
    CHECK_INT(wave_lengths(90, l), 4);
    CHECK_INT(l[3], 90);
}

static void test_set_length(void)
{
    WaveCfg c45 = wave_scaled(CFG, 2700), c15 = wave_scaled(CFG, 900);
    Wave w;
    wave_init(&w, CFG);
    CHECK(wave_set_cfg(&w, c45, 0)); /* idle: the next wave */
    wave_start(&w, 100);
    CHECK_INT(wave_remaining(&w, 100), 2700);

    /* running: keeps the time done, runs to the new length */
    wave_init(&w, CFG);
    wave_start(&w, 0);
    CHECK(wave_set_cfg(&w, c45, 600));
    CHECK_INT(wave_remaining(&w, 600), 2100);
    CHECK_INT(wave_tick(&w, 2699), WEV_NONE);
    CHECK_INT(wave_tick(&w, 2700), WEV_FOCUS_DONE);
    CHECK_INT(wave_remaining(&w, 2700), 540); /* the break fits the new length */

    /* too late to shorten: nothing changes */
    wave_init(&w, CFG);
    wave_start(&w, 0);
    CHECK(!wave_set_cfg(&w, c15, 1000));
    CHECK_INT(wave_remaining(&w, 1000), 500);
    CHECK_INT(w.cfg.focus_s, 1500);
    CHECK(wave_set_cfg(&w, c15, 800)); /* 100 s to go */
    CHECK_INT(wave_remaining(&w, 800), 100);

    /* paused */
    wave_init(&w, CFG);
    wave_start(&w, 0);
    wave_pause(&w, 1000);
    CHECK(wave_set_cfg(&w, c45, 5000));
    CHECK_INT(wave_remaining(&w, 9000), 1700);
    CHECK(!wave_set_cfg(&w, c15, 5000)); /* 1000 s done: past 15 minutes */
    CHECK_INT(wave_remaining(&w, 9000), 1700);

    /* in a break: the break keeps going, the next wave is the new length */
    wave_init(&w, CFG);
    wave_start(&w, 0);
    wave_finish(&w, 1500);
    CHECK(wave_set_cfg(&w, c45, 1600));
    CHECK_INT(wave_remaining(&w, 1600), 200);
    wave_start(&w, 1700);
    CHECK_INT(wave_remaining(&w, 1700), 2700);
}

void suite_wave(void)
{
    test_scaled_breaks();
    test_set_length();
    test_happy_path();
    test_pause_resume();
    test_long_break_cadence();
    test_manual_transitions();
    test_restore_late();
}
