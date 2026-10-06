#ifndef DEW_TASKS_H
#define DEW_TASKS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * In-memory form of tasks.md. The document is a list of sections; each
 * section is a list of items. An item is either a task or a raw line that
 * dew keeps verbatim but never shows. Only the "# Today" and "# Backlog"
 * sections hold visible tasks.
 *
 * Task pointers returned by these functions stay valid only until the next
 * call that adds, removes or moves an item.
 */

typedef enum { LIST_TODAY = 0, LIST_BACKLOG = 1 } ListId;

typedef struct {
    unsigned id;        /* stable for the life of this TaskDoc */
    char *title;        /* never NULL; control characters replaced by spaces */
    char *note;         /* NULL when empty; lines joined by '\n', no indent */
    bool done;
    int waves;
    char done_date[11]; /* "YYYY-MM-DD" or "" */
    int rune;           /* index into RUNE_ART, or RUNE_NONE (-1) until one is given */
    char *extra_meta;   /* unknown "key=value" tokens, space-separated, or NULL */
} Task;

typedef enum { ITEM_TASK, ITEM_RAW } ItemKind;

typedef struct {
    ItemKind kind;
    Task task;  /* when kind == ITEM_TASK */
    char *raw;  /* when kind == ITEM_RAW */
} Item;

typedef enum { SEC_PREAMBLE, SEC_TODAY, SEC_BACKLOG, SEC_DONE, SEC_OTHER } SecKind;

typedef struct {
    SecKind kind;
    char *heading; /* the full heading line, NULL for the preamble */
    Item *items;
    size_t n, cap;
} Section;

typedef struct {
    Section *secs;
    size_t n, cap;
    unsigned next_id;
} TaskDoc;

void doc_init(TaskDoc *d); /* one empty preamble section */
void doc_free(TaskDoc *d);

/* Building blocks used by the parser. */
Section *doc_add_section(TaskDoc *d, SecKind kind, const char *heading);
void section_push_raw(Section *s, const char *line);
Task *section_push_task(TaskDoc *d, Section *s, const char *title);

/* Adds "# Today" and "# Backlog" sections if missing. */
void doc_ensure_lists(TaskDoc *d);

size_t doc_count(const TaskDoc *d, ListId l);
Task *doc_task_at(TaskDoc *d, ListId l, size_t idx);

/* The combined view: Today tasks, then Backlog tasks. */
size_t doc_view_count(const TaskDoc *d);
Task *doc_view_at(TaskDoc *d, size_t i, ListId *list);
long doc_view_index(TaskDoc *d, unsigned id); /* -1 if not found */

Task *doc_find(TaskDoc *d, unsigned id, ListId *list, size_t *idx);
Task *doc_find_title(TaskDoc *d, const char *title, ListId *list, size_t *idx);

Task *doc_add(TaskDoc *d, ListId l, const char *title); /* at the end of the list */
void task_set_title(Task *t, const char *title);
void task_set_note(Task *t, const char *note); /* NULL or "" clears */
void task_toggle_done(Task *t, const char *today);
/* t's next rune in futhark order that no other Today or Backlog task has (just the next one
   when every rune is taken). */
void doc_next_rune(TaskDoc *d, Task *t);

/*
 * Gives every Today and Backlog task that has no rune one, spreading them:
 * each takes a rune the fewest of those tasks have, the first such at or
 * after `start` in futhark order. Returns how many it gave.
 */
int doc_assign_runes(TaskDoc *d, unsigned start);
bool doc_delete(TaskDoc *d, unsigned id);
bool doc_move_list(TaskDoc *d, unsigned id);             /* to the end of the other list */
bool doc_reorder(TaskDoc *d, unsigned id, int delta);    /* +1 down, -1 up; false at an edge */

/*
 * Moves done tasks in Today whose done date is before `today` into
 * "# Done YYYY-MM-DD" sections at the end of the document. Done tasks with
 * no date get today's date and stay. Returns the number of tasks moved.
 */
int doc_archive(TaskDoc *d, const char *today);

#endif
