#include "util.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void oom(void)
{
    fputs("dew: out of memory\n", stderr);
    abort();
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        oom();
    return p;
}

void *xcalloc(size_t n, size_t size)
{
    void *p = calloc(n ? n : 1, size ? size : 1);
    if (!p)
        oom();
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        oom();
    return p;
}

char *xstrndup(const char *s, size_t n)
{
    char *d = xmalloc(n + 1);
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

char *xstrdup(const char *s)
{
    return xstrndup(s, strlen(s));
}

void sb_init(Sbuf *b)
{
    b->buf = NULL;
    b->len = b->cap = 0;
}

static void sb_grow(Sbuf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + extra + 1)
        cap *= 2;
    b->buf = xrealloc(b->buf, cap);
    b->cap = cap;
}

void sb_putn(Sbuf *b, const char *s, size_t n)
{
    sb_grow(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

void sb_puts(Sbuf *b, const char *s)
{
    sb_putn(b, s, strlen(s));
}

void sb_printf(Sbuf *b, const char *fmt, ...)
{
    va_list ap;
    char probe[1]; /* measuring into NULL trips -Wformat-truncation under -O1 */
    va_start(ap, fmt);
    int n = vsnprintf(probe, sizeof probe, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    sb_grow(b, (size_t)n);
    va_start(ap, fmt);
    vsnprintf(b->buf + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
}

char *sb_take(Sbuf *b)
{
    if (!b->buf) {
        sb_grow(b, 0);
        b->buf[0] = '\0';
    }
    char *s = b->buf;
    sb_init(b);
    return s;
}

void sb_free(Sbuf *b)
{
    free(b->buf);
    sb_init(b);
}

void rng_seed(Rng *r, uint64_t seed)
{
    r->s = seed ? seed : 0x9e3779b97f4a7c15ULL;
}

uint32_t rng_next(Rng *r)
{
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

int rng_range(Rng *r, int n)
{
    return n <= 1 ? 0 : (int)(rng_next(r) % (uint32_t)n);
}

double rng_unit(Rng *r)
{
    return rng_next(r) / 4294967296.0;
}

bool parse_int(const char *s, int *out)
{
    const char *p = s;
    bool neg = false;
    if (*p == '-') {
        neg = true;
        p++;
    }
    if (*p < '0' || *p > '9')
        return false;
    long v = 0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9')
            return false;
        v = v * 10 + (*p - '0');
        if (v > INT_MAX)
            return false;
    }
    *out = (int)(neg ? -v : v);
    return true;
}

int utf8_decode(const char *s, uint32_t *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] == 0) {
        *cp = 0;
        return 0;
    }
    if (u[0] < 0x80) {
        *cp = u[0];
        return 1;
    }
    int n;
    uint32_t c;
    if ((u[0] & 0xE0) == 0xC0) {
        n = 2;
        c = u[0] & 0x1F;
    } else if ((u[0] & 0xF0) == 0xE0) {
        n = 3;
        c = u[0] & 0x0F;
    } else if ((u[0] & 0xF8) == 0xF0) {
        n = 4;
        c = u[0] & 0x07;
    } else {
        *cp = 0xFFFD;
        return 1;
    }
    for (int i = 1; i < n; i++) {
        if ((u[i] & 0xC0) != 0x80) {
            *cp = 0xFFFD;
            return 1;
        }
        c = (c << 6) | (u[i] & 0x3F);
    }
    static const uint32_t min[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (c < min[n] || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
        *cp = 0xFFFD;
        return 1;
    }
    *cp = c;
    return n;
}

int utf8_encode(uint32_t cp, char out[4])
{
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int clock_source(void)
{
#ifdef CLOCK_BOOTTIME
    return CLOCK_BOOTTIME;
#else
    return CLOCK_MONOTONIC;
#endif
}

double clock_now(void)
{
    struct timespec ts;
    clock_gettime(clock_source(), &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}
