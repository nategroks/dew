#ifndef DEW_NUDGE_H
#define DEW_NUDGE_H

#include "config.h"

/* A desktop notification through notify-send, if enabled and installed. Never blocks. */
void nudge(const Config *c, const char *body);
void nudge_reap(void); /* collect finished notify-send processes */

#endif
