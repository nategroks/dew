#include "store.h"
#include "test.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[256];

static char *path_in(const char *name)
{
    char *p = malloc(strlen(dir) + strlen(name) + 2);
    sprintf(p, "%s/%s", dir, name);
    return p;
}

static void make_dir(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/dew-test-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        exit(1);
    }
}

static void remove_dir(void)
{
    char cmd[300];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0)
        fprintf(stderr, "could not remove %s\n", dir);
}

static void put(const char *path, const char *text)
{
    CHECK(write_atomic(path, text, strlen(text), false));
}

static void test_write_atomic(void)
{
    char *f = path_in("tasks.md");
    CHECK(write_atomic(f, "one\n", 4, true));
    CHECK(write_atomic(f, "two\n", 4, true));
    char *got = slurp(f, NULL);
    CHECK_STR(got, "two\n");
    free(got);
    char *bak = path_in("tasks.md.bak");
    got = slurp(bak, NULL);
    CHECK_STR(got, "one\n");
    free(got);
    char *tmp = path_in("tasks.md.tmp");
    CHECK(access(tmp, F_OK) != 0); /* no leftover temp file */
    free(f);
    free(bak);
    free(tmp);
}

static void test_write_failures_keep_original(void)
{
    char *missing = path_in("nope/tasks.md");
    CHECK(!write_atomic(missing, "x", 1, true));
    free(missing);

    if (geteuid() == 0)
        return; /* root ignores directory permissions */
    char *ro = path_in("ro");
    CHECK(mkdir(ro, 0755) == 0);
    char *f = path_in("ro/tasks.md");
    put(f, "precious\n");
    CHECK(chmod(ro, 0555) == 0);
    CHECK(!write_atomic(f, "clobber\n", 8, true));
    char *got = slurp(f, NULL);
    CHECK_STR(got, "precious\n");
    free(got);
    CHECK(chmod(ro, 0755) == 0);
    free(f);
    free(ro);
}

static void test_slurp_missing(void)
{
    char *f = path_in("missing");
    errno = 0;
    CHECK(slurp(f, NULL) == NULL);
    CHECK_INT(errno, ENOENT);
    free(f);
}

static void test_stamp(void)
{
    char *f = path_in("stamp");
    CHECK(!file_stamp(f).exists);
    put(f, "a\n");
    FileStamp a = file_stamp(f);
    CHECK(a.exists);
    CHECK(stamp_eq(a, file_stamp(f)));
    put(f, "b\n"); /* same size, new inode */
    CHECK(!stamp_eq(a, file_stamp(f)));
    free(f);
}

static void test_tasks_load_save(void)
{
    char *f = path_in("t1.md");
    TaskDoc d;
    ParseError e;
    FileStamp st;
    CHECK_INT(tasks_load(f, &d, &e, &st), LOAD_CREATED);
    char *got = slurp(f, NULL);
    CHECK_STR(got, "# Today\n\n# Backlog\n");
    free(got);
    doc_add(&d, LIST_TODAY, "write tests");
    CHECK(tasks_save(f, &d, &st));
    doc_free(&d);

    CHECK_INT(tasks_load(f, &d, &e, &st), LOAD_OK);
    CHECK_STR(doc_task_at(&d, LIST_TODAY, 0)->title, "write tests");
    doc_free(&d);

    put(f, "# Today\n- [ ] a <!-- dew waves=x -->\n");
    CHECK_INT(tasks_load(f, &d, &e, &st), LOAD_PARSE_ERROR);
    CHECK_INT(e.line, 2);
    got = slurp(f, NULL);
    CHECK_STR(got, "# Today\n- [ ] a <!-- dew waves=x -->\n"); /* untouched */
    free(got);
    free(f);
}

static void test_reload_if_changed(void)
{
    char *f = path_in("t2.md");
    put(f, "# Today\n- [ ] mine\n\n# Backlog\n");
    TaskDoc d;
    ParseError e;
    FileStamp st;
    CHECK_INT(tasks_load(f, &d, &e, &st), LOAD_OK);
    CHECK_INT(tasks_reload_if_changed(f, &d, &st, &e), 0);

    /* someone else (dew add, or an editor) writes the file */
    put(f, "# Today\n- [ ] mine\n- [ ] theirs\n\n# Backlog\n");
    CHECK_INT(tasks_reload_if_changed(f, &d, &st, &e), 1);
    CHECK_INT(doc_count(&d, LIST_TODAY), 2);
    CHECK_INT(tasks_reload_if_changed(f, &d, &st, &e), 0);

    /* a broken edit is not loaded and the doc survives */
    put(f, "# Today\n- [ ] x <!-- dew done=soon -->\n");
    CHECK_INT(tasks_reload_if_changed(f, &d, &st, &e), -1);
    CHECK_INT(doc_count(&d, LIST_TODAY), 2);
    CHECK_INT(tasks_reload_if_changed(f, &d, &st, &e), -1); /* still broken, still not loaded */
    doc_free(&d);
    free(f);
}

static void test_locks(void)
{
    char *lk = path_in("lock");
    int a = lock_instance(lk);
    CHECK(a >= 0);
    CHECK_INT(lock_instance(lk), -1); /* flock is per open file, so this conflicts */
    lock_file_end(a);
    int b = lock_instance(lk);
    CHECK(b >= 0);
    lock_file_end(b);

    char *f = path_in("t3.md");
    int fd = lock_file_begin(f);
    CHECK(fd >= 0);
    lock_file_end(fd);
    free(lk);
    free(f);
}

static void test_paths(void)
{
    char *xdg = path_in("xdg");
    setenv("XDG_DATA_HOME", xdg, 1);
    setenv("XDG_CONFIG_HOME", xdg, 1);
    Paths p;
    CHECK(paths_init(&p, NULL));
    char *want = path_in("xdg/dew/tasks.md");
    CHECK_STR(p.tasks_path, want);
    struct stat st;
    CHECK(stat(p.garden_dir, &st) == 0 && S_ISDIR(st.st_mode));
    char *g = garden_file(&p, "2026-10-02");
    free(want);
    want = path_in("xdg/dew/garden/2026-10-02");
    CHECK_STR(g, want);
    free(g);
    free(want);
    paths_free(&p);

    CHECK(paths_init(&p, "/elsewhere/list.md"));
    CHECK_STR(p.tasks_path, "/elsewhere/list.md");
    paths_free(&p);
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CONFIG_HOME");
    free(xdg);
}

static void test_state(void)
{
    char *f = path_in("state");
    SavedState s;
    CHECK(!state_load(f, &s));
    SavedState a = {.date = "2026-10-02",
                    .mode = WAVE_PAUSED,
                    .wall_end = 1790950000,
                    .remaining = 812,
                    .long_break = true,
                    .task = "fix grub = theme"};
    CHECK(state_save(f, &a));
    CHECK(state_load(f, &s));
    CHECK_STR(s.date, "2026-10-02");
    CHECK_INT(s.mode, WAVE_PAUSED);
    CHECK_INT(s.wall_end, 1790950000);
    CHECK_INT(s.remaining, 812);
    CHECK(s.long_break);
    CHECK_STR(s.task, "fix grub = theme");
    state_clear(&s);
    put(f, "garbage\n");
    CHECK(!state_load(f, &s));
    state_clear(&s);

    char long_title[301];
    memset(long_title, 'x', 300);
    long_title[300] = '\0';
    SavedState b = {.date = "2026-10-02", .mode = WAVE_FOCUS, .task = long_title};
    CHECK(state_save(f, &b));
    CHECK(state_load(f, &s));
    CHECK_STR(s.task, long_title);
    state_clear(&s);
    free(f);
}

static void test_symlink_and_mode_kept(void)
{
    char *sub = path_in("vault");
    CHECK(mkdir(sub, 0755) == 0);
    char *target = path_in("vault/tasks.md");
    put(target, "# Today\n\n# Backlog\n");
    CHECK(chmod(target, 0644) == 0);
    char *link = path_in("linked.md");
    CHECK(symlink(target, link) == 0);

    CHECK(write_atomic(link, "new\n", 4, true));
    struct stat st;
    CHECK(lstat(link, &st) == 0 && S_ISLNK(st.st_mode)); /* still a link */
    char *got = slurp(target, NULL);
    CHECK_STR(got, "new\n"); /* the target was written */
    free(got);
    CHECK(stat(target, &st) == 0);
    CHECK_INT(st.st_mode & 0777, 0644); /* permissions kept */
    char *bak = path_in("vault/tasks.md.bak");
    got = slurp(bak, NULL);
    CHECK_STR(got, "# Today\n\n# Backlog\n");
    free(got);
    free(bak);
    free(sub);
    free(target);
    free(link);
}

void suite_store(void)
{
    make_dir();
    test_write_atomic();
    test_write_failures_keep_original();
    test_slurp_missing();
    test_stamp();
    test_tasks_load_save();
    test_reload_if_changed();
    test_locks();
    test_paths();
    test_state();
    test_symlink_and_mode_kept();
    remove_dir();
}
