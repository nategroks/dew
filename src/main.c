#include <stdio.h>
#include <string.h>

static void usage(FILE *f)
{
    fputs("usage: dew [--file PATH]            open the TUI\n"
          "       dew [--file PATH] add [-t] TEXT...\n"
          "                                    add a task to the Backlog (-t: Today)\n"
          "       dew --help | --version\n",
          f);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            puts("dew " DEW_VERSION);
            return 0;
        }
    }
    usage(stderr);
    return 2;
}
