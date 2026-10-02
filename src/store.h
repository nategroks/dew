#ifndef DEW_STORE_H
#define DEW_STORE_H

#include "taskfile.h"
#include "tasks.h"
#include "wave.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *data_dir, *config_dir;
    char *tasks_path, *state_path, *garden_dir, *lock_path;
    char *config_path, *colors_path;
} Paths;

/* XDG paths; creates the data and garden directories. False if HOME is unset or mkdir fails. */
bool paths_init(Paths *p, const char *tasks_override);
void paths_free(Paths *p);
char *garden_file(const Paths *p, const char *date); /* malloc'd */

bool mkdir_p(const char *path);

/* Whole file into a malloc'd NUL-terminated buffer. NULL on error (errno set; ENOENT if missing). */
char *slurp(const char *path, size_t *len);

/* path.tmp + fsync + rename. With keep_bak the old file is kept as path.bak. errno set on failure. */
bool write_atomic(const char *path, const char *data, size_t len, bool keep_bak);

typedef struct {
    bool exists;
    long long size, mtime_ns, ino;
} FileStamp;

FileStamp file_stamp(const char *path);
bool stamp_eq(FileStamp a, FileStamp b);

/* Single-instance lock: fd >= 0 on success, -1 if another process holds it, -2 on error. */
int lock_instance(const char *path);
/* Short exclusive lock on path.lock around a read-modify-write of path. Blocks. */
int lock_file_begin(const char *path);
void lock_file_end(int fd);

typedef enum { LOAD_OK, LOAD_CREATED, LOAD_PARSE_ERROR, LOAD_IO_ERROR } LoadResult;

/* Loads (or creates) tasks.md and adds missing Today/Backlog sections. */
LoadResult tasks_load(const char *path, TaskDoc *doc, ParseError *err, FileStamp *st);
bool tasks_save(const char *path, const TaskDoc *doc, FileStamp *st);

/*
 * Reloads doc if the file on disk no longer matches *st.
 * Returns 1 reloaded, 0 unchanged, -1 parse error, -2 read error.
 * On -1/-2 the doc and *st are left as they were.
 */
int tasks_reload_if_changed(const char *path, TaskDoc *doc, FileStamp *st, ParseError *err);

/* The running wave, saved across restarts. */
typedef struct {
    char date[11];
    WaveMode mode;
    long long wall_end;  /* focus/break: wall-clock epoch seconds when it ends */
    double remaining;    /* paused: seconds left */
    bool long_break;
    char task[256];
} SavedState;

bool state_save(const char *path, const SavedState *s);
bool state_load(const char *path, SavedState *s); /* false if missing or unreadable */

#endif
