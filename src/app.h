#ifndef DEW_APP_H
#define DEW_APP_H

#include "config.h"
#include "director.h"
#include "garden.h"
#include "layout.h"
#include "lineedit.h"
#include "palette.h"
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

    Rng fx; /* render jitter and garden placement */
    double now, next_frame, next_check;
} App;

int app_main(const char *tasks_override);

/* render.c */
void render_init_colors(void);
void render_restore_colors(void);
void render(App *a);

#endif
