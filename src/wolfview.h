#ifndef DEW_WOLFVIEW_H
#define DEW_WOLFVIEW_H

#include "app.h"

/* The break wolf in real pixels, on terminals that can show them. render()
   records where the wolf is; these put it on screen around each refresh. */

void wolfview_init(App *a);        /* picks kitty, sixel or braille from config and env */
void wolfview_measure(App *a);     /* reads the cell size in pixels (after a resize) */
bool wolfview_active(const App *a); /* pixels rather than braille dots */
void wolfview_before(App *a);      /* before refresh() */
void wolfview_after(App *a);       /* after refresh() */
void wolfview_hide(App *a);        /* before leaving curses (editor, exit) */
void wolfview_free(App *a);        /* at exit: the terminal forgets the frames */

#endif
