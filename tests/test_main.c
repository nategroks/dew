#include "test.h"

int t_run, t_fail;

void suite_util(void);

static const struct {
    const char *name;
    void (*run)(void);
} SUITES[] = {
    {"util", suite_util},
};

int main(int argc, char **argv)
{
    for (size_t i = 0; i < sizeof SUITES / sizeof *SUITES; i++)
        if (argc < 2 || strcmp(argv[1], SUITES[i].name) == 0)
            SUITES[i].run();
    printf("%d checks, %d failed\n", t_run, t_fail);
    return t_fail ? 1 : 0;
}
