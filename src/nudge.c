#include "nudge.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

void nudge(const Config *c, const char *body)
{
    if (!c->notify)
        return;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    char *argv[] = {"notify-send", "-a", "dew", "dew", (char *)body, NULL};
    pid_t pid;
    posix_spawnp(&pid, "notify-send", &fa, NULL, argv, environ); /* missing notify-send is fine */
    posix_spawn_file_actions_destroy(&fa);
}

void nudge_reap(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}
