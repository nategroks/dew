#ifndef DEW_TASKFILE_H
#define DEW_TASKFILE_H

#include "tasks.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t line;   /* 1-based */
    char msg[96];
} ParseError;

/*
 * Parses tasks.md text into a fresh TaskDoc (the caller must not have
 * initialized it). On failure the doc is freed, *err says why, and false
 * is returned. Does not add missing Today/Backlog sections.
 */
bool taskfile_parse(const char *text, size_t len, TaskDoc *doc, ParseError *err);

/* Returns a malloc'd NUL-terminated string; *len_out (if not NULL) gets its length. */
char *taskfile_serialize(const TaskDoc *doc, size_t *len_out);

#endif
