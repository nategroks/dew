#include "store.h"

#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static char *join(const char *a, const char *b)
{
    Sbuf s;
    sb_init(&s);
    sb_printf(&s, "%s/%s", a, b);
    return sb_take(&s);
}

static char *suffixed(const char *path, const char *suffix)
{
    Sbuf s;
    sb_init(&s);
    sb_printf(&s, "%s%s", path, suffix);
    return sb_take(&s);
}

bool mkdir_p(const char *path)
{
    char *p = xstrdup(path);
    bool ok = true;
    for (char *c = p + 1; ok && *c; c++) {
        if (*c != '/')
            continue;
        *c = '\0';
        ok = mkdir(p, 0755) == 0 || errno == EEXIST;
        *c = '/';
    }
    if (ok)
        ok = mkdir(p, 0755) == 0 || errno == EEXIST;
    free(p);
    return ok;
}

bool paths_init(Paths *p, const char *tasks_override)
{
    memset(p, 0, sizeof *p);
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    const char *xd = getenv("XDG_DATA_HOME"), *xc = getenv("XDG_CONFIG_HOME");
    char *base;
    if (xd && *xd)
        p->data_dir = join(xd, "dew");
    else {
        base = join(home, ".local/share");
        p->data_dir = join(base, "dew");
        free(base);
    }
    if (xc && *xc)
        p->config_dir = join(xc, "dew");
    else {
        base = join(home, ".config");
        p->config_dir = join(base, "dew");
        free(base);
    }
    p->tasks_path = tasks_override ? xstrdup(tasks_override) : join(p->data_dir, "tasks.md");
    p->state_path = join(p->data_dir, "state");
    p->garden_dir = join(p->data_dir, "garden");
    p->lock_path = join(p->data_dir, "lock");
    p->config_path = join(p->config_dir, "config");
    p->colors_path = join(p->config_dir, "colors");
    return mkdir_p(p->data_dir) && mkdir_p(p->garden_dir);
}

void paths_free(Paths *p)
{
    free(p->data_dir);
    free(p->config_dir);
    free(p->tasks_path);
    free(p->state_path);
    free(p->garden_dir);
    free(p->lock_path);
    free(p->config_path);
    free(p->colors_path);
    memset(p, 0, sizeof *p);
}

char *garden_file(const Paths *p, const char *date)
{
    return join(p->garden_dir, date);
}

char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    Sbuf b;
    sb_init(&b);
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        sb_putn(&b, buf, n);
    bool bad = ferror(f);
    fclose(f);
    if (bad) {
        sb_free(&b);
        errno = EIO;
        return NULL;
    }
    if (len)
        *len = b.len;
    return sb_take(&b);
}

bool write_atomic(const char *path, const char *data, size_t len, bool keep_bak)
{
    char *tmp = suffixed(path, ".tmp");
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        free(tmp);
        return false;
    }
    bool ok = true;
    size_t off = 0;
    while (ok && off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w < 0 && errno == EINTR)
            continue;
        if (w < 0)
            ok = false;
        else
            off += (size_t)w;
    }
    if (ok && fsync(fd) != 0)
        ok = false;
    if (close(fd) != 0)
        ok = false;
    if (ok && keep_bak) {
        char *bak = suffixed(path, ".bak");
        unlink(bak);
        if (link(path, bak) != 0 && errno != ENOENT) {
            /* no .bak is not worth failing a save over */
        }
        free(bak);
    }
    if (ok && rename(tmp, path) != 0)
        ok = false;
    if (!ok) {
        int e = errno;
        unlink(tmp);
        errno = e;
    }
    free(tmp);
    return ok;
}

FileStamp file_stamp(const char *path)
{
    FileStamp s = {0};
    struct stat st;
    if (stat(path, &st) == 0) {
        s.exists = true;
        s.size = (long long)st.st_size;
        s.mtime_ns = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
        s.ino = (long long)st.st_ino;
    }
    return s;
}

bool stamp_eq(FileStamp a, FileStamp b)
{
    return a.exists == b.exists && a.size == b.size && a.mtime_ns == b.mtime_ns && a.ino == b.ino;
}

int lock_instance(const char *path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0)
        return -2;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        int busy = errno == EWOULDBLOCK;
        close(fd);
        return busy ? -1 : -2;
    }
    return fd;
}

int lock_file_begin(const char *path)
{
    char *lp = suffixed(path, ".lock");
    int fd = open(lp, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    free(lp);
    if (fd < 0)
        return -1;
    while (flock(fd, LOCK_EX) != 0) {
        if (errno != EINTR) {
            close(fd);
            return -1;
        }
    }
    return fd;
}

void lock_file_end(int fd)
{
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

bool tasks_save(const char *path, const TaskDoc *doc, FileStamp *st)
{
    size_t len;
    char *text = taskfile_serialize(doc, &len);
    bool ok = write_atomic(path, text, len, true);
    free(text);
    if (ok && st)
        *st = file_stamp(path);
    return ok;
}

LoadResult tasks_load(const char *path, TaskDoc *doc, ParseError *err, FileStamp *st)
{
    FileStamp before = file_stamp(path);
    size_t len;
    char *text = slurp(path, &len);
    if (!text) {
        if (errno != ENOENT)
            return LOAD_IO_ERROR;
        doc_init(doc);
        doc_ensure_lists(doc);
        if (!tasks_save(path, doc, st)) {
            int e = errno;
            doc_free(doc);
            errno = e;
            return LOAD_IO_ERROR;
        }
        return LOAD_CREATED;
    }
    bool ok = taskfile_parse(text, len, doc, err);
    free(text);
    if (!ok)
        return LOAD_PARSE_ERROR;
    doc_ensure_lists(doc);
    if (st)
        *st = before;
    return LOAD_OK;
}

int tasks_reload_if_changed(const char *path, TaskDoc *doc, FileStamp *st, ParseError *err)
{
    FileStamp now = file_stamp(path);
    if (stamp_eq(now, *st))
        return 0;
    size_t len;
    char *text = slurp(path, &len);
    if (!text)
        return -2;
    TaskDoc fresh;
    bool ok = taskfile_parse(text, len, &fresh, err);
    free(text);
    if (!ok)
        return -1;
    doc_ensure_lists(&fresh);
    doc_free(doc);
    *doc = fresh;
    *st = now;
    return 1;
}

static const char *MODE_NAMES[] = {"idle", "focus", "paused", "break"};

bool state_save(const char *path, const SavedState *s)
{
    Sbuf b;
    sb_init(&b);
    sb_printf(&b, "date=%s\nmode=%s\nend=%lld\nremaining=%.0f\nlong=%d\ntask=%s\n", s->date,
              MODE_NAMES[s->mode], s->wall_end, s->remaining, s->long_break ? 1 : 0, s->task);
    bool ok = write_atomic(path, b.buf, b.len, false);
    sb_free(&b);
    return ok;
}

bool state_load(const char *path, SavedState *s)
{
    memset(s, 0, sizeof *s);
    char *text = slurp(path, NULL);
    if (!text)
        return false;
    bool have_mode = false;
    for (char *save = NULL, *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char *k = line, *v = eq + 1;
        if (strcmp(k, "date") == 0)
            snprintf(s->date, sizeof s->date, "%.10s", v);
        else if (strcmp(k, "mode") == 0) {
            for (int m = 0; m < 4; m++)
                if (strcmp(v, MODE_NAMES[m]) == 0) {
                    s->mode = (WaveMode)m;
                    have_mode = true;
                }
        } else if (strcmp(k, "end") == 0)
            s->wall_end = strtoll(v, NULL, 10);
        else if (strcmp(k, "remaining") == 0)
            s->remaining = strtod(v, NULL);
        else if (strcmp(k, "long") == 0)
            s->long_break = strcmp(v, "1") == 0;
        else if (strcmp(k, "task") == 0)
            snprintf(s->task, sizeof s->task, "%s", v);
    }
    free(text);
    return have_mode;
}
