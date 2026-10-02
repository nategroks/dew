#ifndef DEW_LINEEDIT_H
#define DEW_LINEEDIT_H

#include <stdint.h>

/* A one-line text editor for the add / edit prompt. Code points, no locale. */

#define LE_MAX 256

enum {
    LE_KEY_LEFT = 0x110000, /* above the Unicode range */
    LE_KEY_RIGHT,
    LE_KEY_HOME,
    LE_KEY_END,
    LE_KEY_BACKSPACE,
    LE_KEY_DELETE,
};

typedef enum { LE_CONTINUE, LE_COMMIT, LE_CANCEL } LeResult;

typedef struct {
    uint32_t buf[LE_MAX];
    int len, cur;
} LineEdit;

void le_init(LineEdit *e, const char *utf8);
/* key: a code point, or LE_KEY_*. Enter commits, Esc cancels, ^U clears to the start. */
LeResult le_key(LineEdit *e, uint32_t key);
char *le_text(const LineEdit *e);  /* malloc'd UTF-8 */
char *le_before_cursor(const LineEdit *e); /* malloc'd UTF-8, for placing the cursor */

#endif
