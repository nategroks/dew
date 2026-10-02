#ifndef DEW_DIRECTOR_H
#define DEW_DIRECTOR_H

#include "garden.h"
#include "life.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Turns what you do into what the Life board does. Pure: no clock, no
 * curses. The app calls director_frame() fps times a second and the
 * director_on_* functions when things happen.
 */

typedef enum { MOOD_IDLE, MOOD_FOCUS, MOOD_PAUSED, MOOD_BREAK } Mood;
typedef enum { THEN_BRAIN, THEN_GARDEN } SweepThen;

typedef struct {
    Life life;
    Rng rng;
    const Garden *garden;
    Mood mood;
    long frame;
    int tear;          /* frames of glitch tear left */
    int excite;        /* idle: frames of fast stepping left after a fleet */
    int highlife_left; /* focus: frames of HighLife left */
    int pending_sweep; /* frames until a break sweep starts, 0 = none */
    bool sweeping;
    double sweep_x;    /* the river's leading edge */
    double river_seed; /* shapes this river's meanders */
    int river_fade;    /* frames left of the river fading out, after it has crossed */
    SweepThen then;
    uint8_t tint_cursor;
    Rule focus_rule; /* what focus waves run; Conway unless the user picks another */
    GardenBox *boxes; /* where the garden's plants sit on the board, for coloring */
    size_t nboxes;
} Director;

void director_init(Director *d, int w, int h, uint64_t seed, const Garden *g);
void director_free(Director *d);
void director_resize(Director *d, int w, int h);

void director_on_wave_start(Director *d);
void director_on_pause(Director *d);
void director_on_resume(Director *d);
void director_on_task_done(Director *d);
void director_on_wave_finish(Director *d);
void director_on_break_end(Director *d);
void director_on_abandon(Director *d);
void director_show_garden(Director *d); /* clear and stamp the garden */
/* Idle: applies to the next wave. Focus/paused: switches the running board, with a tear. */
void director_set_focus_rule(Director *d, Rule r);

void director_frame(Director *d);
bool director_animating(const Director *d); /* false when only the slow idle step runs */
/* While sweeping: the river's centerline at column x and its half width, in board cells. */
double director_river_y(const Director *d, int x);
int director_river_half(const Director *d);
/* While the river crosses: where the wolf is (top-left, board cells) and its run-cycle frame.
   False when it isn't on screen or the board is too small for it. */
bool director_wolf(const Director *d, int *x, int *y, int *frame);

#endif
