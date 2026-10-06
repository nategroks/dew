#ifndef DEW_APP_H
#define DEW_APP_H

#include "config.h"
#include "director.h"
#include "garden.h"
#include "layout.h"
#include "lineedit.h"
#include "palette.h"
#include "runes.h"
#include "store.h"
#include "tasks.h"
#include "wave.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum { PROMPT_NONE, PROMPT_ADD, PROMPT_EDIT, PROMPT_DELETE } PromptKind;

typedef struct App {
    Config cfg;
    Paths paths;
    PaletteRGB rgb;
    int instance_fd;

    TaskDoc doc;
    FileStamp stamp;
    bool file_broken; /* tasks.md changed on disk and doesn't parse: don't save */
    size_t broken_line;
    size_t sel, scroll;

    Wave wave;
    int length_min;        /* focus length for waves, picked with m */
    unsigned active_id;    /* the task the wave is on (0 = none) */
    char *active_title;    /* heap; NULL when the wave has no task */
    char today[11];

    Garden garden;
    Director dir;
    bool garden_view;
    Life gview; /* the full-screen garden while garden_view */
    GardenBox *gboxes; /* its plants' positions, for coloring */
    size_t ngboxes;

    Layout lay;
    int cols, rows;
    bool help, quit;
    PromptKind prompt;
    LineEdit le;
    ListId add_list;
    unsigned prompt_id;    /* the task an edit/delete prompt was opened on */
    char *prompt_title;
    int cur_y, cur_x; /* where the prompt cursor goes */

    char msg[200];
    double msg_until;
    bool msg_warn;

    /* the wolves in real pixels (wolfview.c) */
    GfxMode gfx;          /* braille, kitty or sixel */
    int cell_w, cell_h;   /* pixels per cell; 0 when the terminal doesn't say */
    struct {
        bool show;        /* render() saw the wolf this frame */
        int x_px, row;    /* its left edge in screen pixels, its top screen row */
        int frame, kind;
        int rune, rune_x_px, rune_row; /* the rune it chases (RUNE_NONE), where */
        Rect pane;        /* the Life board it is clipped to */
    } wolf;
    struct {
        bool shown, rune_shown;
        unsigned id, rune_id;
        int row, rows;    /* the rows both cover */
    } wolf_prev;          /* what is on screen now */
    int rune_up_w[RUNE_ART_COUNT], rune_up_h[RUNE_ART_COUNT]; /* rune images kitty holds */
    int wolf_up_w[WOLF_KINDS], wolf_up_h[WOLF_KINDS]; /* frames kitty holds, per coat (0 = none yet) */
    bool wolf_sync;           /* a synchronized update is open */

    Rng fx; /* render jitter and garden placement */
    double now, next_frame, next_check;
} App;

int app_main(const char *tasks_override);

/* render.c */
void render_init_colors(void);
void render_restore_colors(void);
void render(App *a);

#endif
