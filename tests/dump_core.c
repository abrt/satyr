#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if defined _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static char const *prefix = "/tmp/satyr.core";

#if defined(__clang__)
__attribute__((optnone))  
#else
__attribute__((optimize((0))))
#endif
int
dump_core(int    depth,
          char **name)
{
    pid_t pid;
    char *pid_string;
    pid_t fork_pid;
    int status;

    if (--depth > 0)
    {
        return dump_core(depth, name);
    }

    pid = getpid();

    if (asprintf(&pid_string, "%d", pid) == -1)
    {
        return false;
    }

    fork_pid = fork();
    if (-1 == fork_pid)
    {
        free(pid_string);

        return false;
    }
    else if (0 == fork_pid)
    {
        char const *const argv[] =
        {
            "gcore",
            "-o", prefix,
            pid_string,
            NULL,
        };
        int fd;

        fd = open("/dev/null", O_WRONLY);

        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);

        close(fd);

        execv("/usr/bin/gcore", (char *const *) argv);
    }

    if (waitpid(fork_pid, &status, 0) == -1)
    {
        free(pid_string);

        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        return false;
    }

    if (NULL == *name)
    {
        if (asprintf(name, "%s.%s", prefix, pid_string) == -1)
        {
            return false;
        }
    }

    free(pid_string);

    return true;
}

int
main (int    argc,
      char **argv)
{
    int depth;
    char *name = NULL;

    depth = atoi(argv[1]);

    if (!dump_core(depth, &name))
    {
        return EXIT_FAILURE;
    }

    printf("%s", name);

    return EXIT_SUCCESS;
}
