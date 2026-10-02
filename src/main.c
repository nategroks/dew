#include "store.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f)
{
    fputs("usage: dew [--file PATH]            open the TUI\n"
          "       dew [--file PATH] add [-t] TEXT...\n"
          "                                    add a task to the Backlog (-t: Today)\n"
          "       dew --help | --version\n",
          f);
}

static int cmd_add(int argc, char **argv, const char *file)
{
    int i = 0;
    bool today = false;
    if (i < argc && strcmp(argv[i], "-t") == 0) {
        today = true;
        i++;
    }
    Sbuf title;
    sb_init(&title);
    for (; i < argc; i++) {
        if (title.len)
            sb_puts(&title, " ");
        sb_puts(&title, argv[i]);
    }
    bool blank = true;
    for (size_t k = 0; k < title.len; k++)
        blank &= title.buf[k] == ' ' || title.buf[k] == '\t';
    if (blank) {
        sb_free(&title);
        fputs("dew add: give the task some text, e.g. dew add fix the grub theme\n", stderr);
        return 2;
    }

    Paths p;
    if (!paths_init(&p, file)) {
        fprintf(stderr, "dew: cannot set up %s: %s\n", p.data_dir ? p.data_dir : "data directory",
                strerror(errno));
        paths_free(&p);
        sb_free(&title);
        return 1;
    }
    int rc = 0;
    int lk = lock_file_begin(p.tasks_path);
    TaskDoc d;
    ParseError e;
    FileStamp st;
    switch (tasks_load(p.tasks_path, &d, &e, &st)) {
    case LOAD_PARSE_ERROR:
        fprintf(stderr, "dew: %s:%zu: %s\n", p.tasks_path, e.line, e.msg);
        rc = 1;
        break;
    case LOAD_IO_ERROR:
        fprintf(stderr, "dew: cannot read %s: %s\n", p.tasks_path, strerror(errno));
        rc = 1;
        break;
    default:
        doc_add(&d, today ? LIST_TODAY : LIST_BACKLOG, title.buf);
        if (!tasks_save(p.tasks_path, &d, &st)) {
            fprintf(stderr, "dew: cannot save %s: %s\n", p.tasks_path, strerror(errno));
            rc = 1;
        } else {
            printf("added to %s: %s\n", today ? "Today" : "Backlog", title.buf);
        }
        doc_free(&d);
    }
    lock_file_end(lk);
    paths_free(&p);
    sb_free(&title);
    return rc;
}

int main(int argc, char **argv)
{
    const char *file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            puts("dew " DEW_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--file") == 0) {
            if (++i >= argc) {
                usage(stderr);
                return 2;
            }
            file = argv[i];
            continue;
        }
        if (strcmp(argv[i], "add") == 0)
            return cmd_add(argc - i - 1, argv + i + 1, file);
        fprintf(stderr, "dew: unknown argument \"%s\"\n", argv[i]);
        usage(stderr);
        return 2;
    }
    usage(stderr);
    return 2;
}
