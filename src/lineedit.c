#include "lineedit.h"

#include "util.h"

#include <string.h>

void le_init(LineEdit *e, const char *utf8)
{
    e->len = e->cur = 0;
    uint32_t cp;
    int n;
    while (e->len < LE_MAX && (n = utf8_decode(utf8, &cp)) > 0) {
        utf8 += n;
        if (cp >= 0x20 && cp != 0x7f)
            e->buf[e->len++] = cp;
    }
    e->cur = e->len;
}

static void erase(LineEdit *e, int at)
{
    memmove(&e->buf[at], &e->buf[at + 1], (size_t)(e->len - at - 1) * sizeof *e->buf);
    e->len--;
}

LeResult le_key(LineEdit *e, uint32_t key)
{
    switch (key) {
    case '\n':
    case '\r':
        return LE_COMMIT;
    case 27:
        return LE_CANCEL;
    case LE_KEY_LEFT:
        if (e->cur > 0)
            e->cur--;
        break;
    case LE_KEY_RIGHT:
        if (e->cur < e->len)
            e->cur++;
        break;
    case LE_KEY_HOME:
    case 1: /* ^A */
        e->cur = 0;
        break;
    case LE_KEY_END:
    case 5: /* ^E */
        e->cur = e->len;
        break;
    case LE_KEY_BACKSPACE:
    case 8:
    case 127:
        if (e->cur > 0)
            erase(e, --e->cur);
        break;
    case LE_KEY_DELETE:
        if (e->cur < e->len)
            erase(e, e->cur);
        break;
    case 21: /* ^U */
        memmove(e->buf, &e->buf[e->cur], (size_t)(e->len - e->cur) * sizeof *e->buf);
        e->len -= e->cur;
        e->cur = 0;
        break;
    default:
        if (key < 0x20 || key == 0x7f || key > 0x10FFFF || e->len >= LE_MAX)
            break;
        memmove(&e->buf[e->cur + 1], &e->buf[e->cur], (size_t)(e->len - e->cur) * sizeof *e->buf);
        e->buf[e->cur++] = key;
        e->len++;
    }
    return LE_CONTINUE;
}

static char *encode(const uint32_t *cps, int n)
{
    Sbuf b;
    sb_init(&b);
    for (int i = 0; i < n; i++) {
        char out[4];
        sb_putn(&b, out, (size_t)utf8_encode(cps[i], out));
    }
    return sb_take(&b);
}

char *le_text(const LineEdit *e)
{
    return encode(e->buf, e->len);
}

char *le_before_cursor(const LineEdit *e)
{
    return encode(e->buf, e->cur);
}
