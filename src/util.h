#ifndef DEW_UTIL_H
#define DEW_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Allocation that aborts on out-of-memory. */
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Growable, always NUL-terminated byte buffer. */
typedef struct {
    char *buf;
    size_t len, cap;
} Sbuf;

void sb_init(Sbuf *b);
void sb_putn(Sbuf *b, const char *s, size_t n);
void sb_puts(Sbuf *b, const char *s);
void sb_printf(Sbuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
char *sb_take(Sbuf *b); /* returns the malloc'd string ("" if empty) and resets b */
void sb_free(Sbuf *b);

/* Small deterministic PRNG (xorshift64*). */
typedef struct {
    uint64_t s;
} Rng;

void rng_seed(Rng *r, uint64_t seed);
uint32_t rng_next(Rng *r);
int rng_range(Rng *r, int n); /* [0, n); returns 0 when n <= 1 */
double rng_unit(Rng *r);      /* [0, 1) */

/* Strict decimal integer: optional '-', digits only, no surrounding text. */
bool parse_int(const char *s, int *out);

/* UTF-8 without locale dependence. Invalid bytes decode as U+FFFD and consume 1 byte. */
int utf8_decode(const char *s, uint32_t *cp); /* bytes consumed, 0 at NUL */
int utf8_encode(uint32_t cp, char out[4]);    /* bytes written */

#endif
