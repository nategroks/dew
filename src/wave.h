#ifndef DEW_WAVE_H
#define DEW_WAVE_H

#include <stdbool.h>

/*
 * The wave (pomodoro) state machine. It never reads a clock: every call
 * takes `now` in seconds from any monotonic source.
 *
 *   idle --start--> focus <--pause/resume--> paused
 *   focus/paused --finish (or focus time runs out)--> break --(time runs out)--> idle
 *   break --start--> focus     (skips the rest of the break)
 *   focus/paused --abandon--> idle   (not counted)
 */

typedef enum { WAVE_IDLE, WAVE_FOCUS, WAVE_PAUSED, WAVE_BREAK } WaveMode;
typedef enum { WEV_NONE, WEV_FOCUS_DONE, WEV_BREAK_DONE } WaveEvent;

typedef struct {
    int focus_s, short_s, long_s, long_every;
} WaveCfg;

typedef struct {
    WaveMode mode;
    WaveCfg cfg;
    double end;       /* when focus or break ends */
    double remaining; /* seconds left while paused */
    int waves_today;  /* finished waves; set by the app from today's garden */
    bool long_break;  /* the current break is a long one */
} Wave;

/* What survives a restart. `remaining` may be negative when restored late. */
typedef struct {
    WaveMode mode;
    double remaining;
    bool long_break;
} WaveSnap;

void wave_init(Wave *w, WaveCfg cfg);
bool wave_start(Wave *w, double now);
bool wave_pause(Wave *w, double now);
bool wave_resume(Wave *w, double now);
bool wave_finish(Wave *w, double now); /* counts the wave and starts the break */
bool wave_abandon(Wave *w);
bool wave_end_break(Wave *w);

/*
 * Applies at most one automatic transition and reports it. Call it until
 * it returns WEV_NONE. A focus that ran out starts its break at the moment
 * focus ended, not at `now`, so a late restore can finish both.
 */
WaveEvent wave_tick(Wave *w, double now);
double wave_remaining(const Wave *w, double now); /* >= 0 */

WaveSnap wave_snapshot(const Wave *w, double now);
void wave_restore(Wave *w, WaveSnap s, double now);

#endif
