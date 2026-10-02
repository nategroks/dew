#include "test.h"

int t_run, t_fail;

void suite_util(void);
void suite_tasks(void);
void suite_taskfile(void);
void suite_config(void);
void suite_wave(void);
void suite_store(void);
void suite_life(void);
void suite_garden(void);
void suite_director(void);
void suite_view(void);

static const struct {
    const char *name;
    void (*run)(void);
} SUITES[] = {
    {"util", suite_util},
    {"tasks", suite_tasks},
    {"taskfile", suite_taskfile},
    {"config", suite_config},
    {"wave", suite_wave},
    {"store", suite_store},
    {"life", suite_life},
    {"garden", suite_garden},
    {"director", suite_director},
    {"view", suite_view},
};

int main(int argc, char **argv)
{
    for (size_t i = 0; i < sizeof SUITES / sizeof *SUITES; i++)
        if (argc < 2 || strcmp(argv[1], SUITES[i].name) == 0)
            SUITES[i].run();
    printf("%d checks, %d failed\n", t_run, t_fail);
    return t_fail ? 1 : 0;
}
