#ifndef DEW_TEST_H
#define DEW_TEST_H

#include <stdio.h>
#include <string.h>

extern int t_run, t_fail;

#define CHECK(c)                                                                  \
    do {                                                                          \
        t_run++;                                                                  \
        if (!(c)) {                                                               \
            t_fail++;                                                             \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); \
        }                                                                         \
    } while (0)

#define CHECK_INT(a, b)                                                           \
    do {                                                                          \
        t_run++;                                                                  \
        long long a_ = (long long)(a), b_ = (long long)(b);                       \
        if (a_ != b_) {                                                           \
            t_fail++;                                                             \
            fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__,       \
                    __LINE__, #a, a_, b_);                                        \
        }                                                                         \
    } while (0)

#define CHECK_STR(a, b)                                                           \
    do {                                                                          \
        t_run++;                                                                  \
        const char *a_ = (a), *b_ = (b);                                          \
        if (!a_ || !b_ || strcmp(a_, b_) != 0) {                                  \
            t_fail++;                                                             \
            fprintf(stderr, "%s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__,   \
                    __LINE__, #a, a_ ? a_ : "(null)", b_ ? b_ : "(null)");        \
        }                                                                         \
    } while (0)

#endif
