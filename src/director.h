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

#define RUN_QUEUE 4 /* task wolves waiting for their turn */
#define RUNE_GAP 3  /* board cells between a wolf's nose and the rune it chases */

/* A wolf on the board: top-left in board cells, run-cycle frame, coat (WOLF_SNOW ...), and the
   task's rune it chases (RUNE_NONE, or an index into RUNE_ART), which runs RUNE_GAP cells ahead
   of its nose, RUNE_ART_W x RUNE_ART_H cells, at height y + 2. */
typedef struct {
    int x, y, frame, kind, rune;
} WolfSpot;

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
    struct {
        bool on;
        double x;   /* left edge, board cells; starts off the left side */
        double y01; /* height: 0 top .. 1 bottom, so it survives a resize */
        int kind, rune;
        long start; /* the frame it set off */
    } run;          /* a task wolf crossing the board */
    struct {
        uint8_t kind;
        int8_t rune;
    } run_queue[RUN_QUEUE];
    int run_queued;
    int wolf_next;  /* rotates the task wolves' coats */
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
void director_on_task_done(Director *d, int rune); /* the task's rune, or RUNE_NONE */
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
/* The wolf on the board: the snow wolf while the river crosses, else a task wolf (one runs
   across for each task done, in turn, never during the river). False when none is on screen
   or the board is too small for one. */
bool director_wolf(const Director *d, WolfSpot *w);

#endif
