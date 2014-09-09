#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include "process.h"

void drop_privileges()
{
    struct passwd *user;
    struct rlimit limit;

    if (!geteuid()) {
        user = getpwnam("nobody");
        if (!user) {
            perror("getpwnam");
            exit(EXIT_FAILURE);
        }
        if (chroot("/var/empty")) {
            perror("chroot");
            exit(EXIT_FAILURE);
        }
        if (chdir("/")) {
            perror("chdir");
            exit(EXIT_FAILURE);
        }
        if (setresgid(user->pw_gid, user->pw_gid, user->pw_gid)) {
            perror("setresgid");
            exit(EXIT_FAILURE);
        }
        if (setgroups(1, &user->pw_gid)) {
            perror("setgroups");
            exit(EXIT_FAILURE);
        }
        if (setresuid(user->pw_uid, user->pw_uid, user->pw_uid)) {
            perror("setresuid");
            exit(EXIT_FAILURE);
        }
    }
    limit.rlim_cur = limit.rlim_max = 8192;
    setrlimit(RLIMIT_DATA, &limit);
    setrlimit(RLIMIT_MEMLOCK, &limit);
    setrlimit(RLIMIT_AS, &limit);
    setrlimit(RLIMIT_STACK, &limit);
    limit.rlim_cur = limit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &limit);
    setrlimit(RLIMIT_NPROC, &limit);
    if (!geteuid() || !getegid()) {
        fprintf(stderr, "Mysteriously still running as root... Goodbye.\n");
        exit(EXIT_FAILURE);
    }
}

void set_process_name(const char *name, int argc, char *argv[])
{
        char *start, *end;

        prctl(PR_SET_NAME, name);
        end = argv[argc - 1] + strlen(argv[argc - 1]);
        strcpy(argv[0], name);
        start = argv[0] + strlen(argv[0]);
        while (start < end)
                *(start++) = '\0';
}
