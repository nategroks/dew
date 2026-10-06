#include "wave.h"

#include <math.h>
#include <string.h>

int wave_lengths(int config_min, int out[WAVE_LENGTHS_MAX])
{
    static const int PRESETS[] = {15, 25, 45};
    int n = 0;
    bool placed = false;
    for (int i = 0; i < 3; i++) {
        if (!placed && config_min <= PRESETS[i]) {
            placed = true;
            if (config_min < PRESETS[i])
                out[n++] = config_min;
        }
        out[n++] = PRESETS[i];
    }
    if (!placed)
        out[n++] = config_min;
    return n;
}

static int scale_min(int s, int focus_s, int base_focus_s)
{
    long m = lround((double)s * focus_s / base_focus_s / 60.0);
    return (int)(m < 1 ? 1 : m) * 60;
}

WaveCfg wave_scaled(WaveCfg base, int focus_s)
{
    WaveCfg c = base;
    c.focus_s = focus_s;
    if (base.focus_s > 0) {
        c.short_s = scale_min(base.short_s, focus_s, base.focus_s);
        c.long_s = scale_min(base.long_s, focus_s, base.focus_s);
    }
    return c;
}

void wave_init(Wave *w, WaveCfg cfg)
{
    memset(w, 0, sizeof *w);
    w->cfg = cfg;
    w->mode = WAVE_IDLE;
}

bool wave_set_cfg(Wave *w, WaveCfg cfg, double now)
{
    double change = cfg.focus_s - w->cfg.focus_s;
    if (w->mode == WAVE_FOCUS) {
        if (w->end + change <= now)
            return false;
        w->end += change;
    } else if (w->mode == WAVE_PAUSED) {
        if (w->remaining + change <= 0)
            return false;
        w->remaining += change;
    }
    w->cfg = cfg;
    return true;
}

static void begin_break(Wave *w, double from)
{
    w->waves_today++;
    w->long_break = w->cfg.long_every > 0 && w->waves_today % w->cfg.long_every == 0;
    w->mode = WAVE_BREAK;
    w->end = from + (w->long_break ? w->cfg.long_s : w->cfg.short_s);
}

bool wave_start(Wave *w, double now)
{
    if (w->mode != WAVE_IDLE && w->mode != WAVE_BREAK)
        return false;
    w->mode = WAVE_FOCUS;
    w->end = now + w->cfg.focus_s;
    return true;
}

bool wave_pause(Wave *w, double now)
{
    if (w->mode != WAVE_FOCUS)
        return false;
    w->remaining = w->end - now;
    if (w->remaining < 0)
        w->remaining = 0;
    w->mode = WAVE_PAUSED;
    return true;
}

bool wave_resume(Wave *w, double now)
{
    if (w->mode != WAVE_PAUSED)
        return false;
    w->mode = WAVE_FOCUS;
    w->end = now + w->remaining;
    return true;
}

bool wave_finish(Wave *w, double now)
{
    if (w->mode != WAVE_FOCUS && w->mode != WAVE_PAUSED)
        return false;
    begin_break(w, now);
    return true;
}

bool wave_abandon(Wave *w)
{
    if (w->mode != WAVE_FOCUS && w->mode != WAVE_PAUSED)
        return false;
    w->mode = WAVE_IDLE;
    return true;
}

bool wave_end_break(Wave *w)
{
    if (w->mode != WAVE_BREAK)
        return false;
    w->mode = WAVE_IDLE;
    return true;
}

WaveEvent wave_tick(Wave *w, double now)
{
    if (w->mode == WAVE_FOCUS && now >= w->end) {
        begin_break(w, w->end);
        return WEV_FOCUS_DONE;
    }
    if (w->mode == WAVE_BREAK && now >= w->end) {
        w->mode = WAVE_IDLE;
        return WEV_BREAK_DONE;
    }
    return WEV_NONE;
}

double wave_remaining(const Wave *w, double now)
{
    double r;
    switch (w->mode) {
    case WAVE_FOCUS:
    case WAVE_BREAK:
        r = w->end - now;
        break;
    case WAVE_PAUSED:
        r = w->remaining;
        break;
    default:
        r = 0;
    }
    return r > 0 ? r : 0;
}

WaveSnap wave_snapshot(const Wave *w, double now)
{
    WaveSnap s = {w->mode, 0, w->long_break};
    if (w->mode == WAVE_PAUSED)
        s.remaining = w->remaining;
    else if (w->mode == WAVE_FOCUS || w->mode == WAVE_BREAK)
        s.remaining = w->end - now;
    return s;
}

void wave_restore(Wave *w, WaveSnap s, double now)
{
    w->mode = s.mode;
    w->long_break = s.long_break;
    if (s.mode == WAVE_PAUSED)
        w->remaining = s.remaining;
    else
        w->end = now + s.remaining;
}
