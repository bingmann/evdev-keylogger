/*******************************************************************************
 * process.c
 *
 * Drop privileges and optionally change process name.
 *
 *******************************************************************************
 * Copyright (C) 2012 Jason A. Donenfeld <Jason@zx2c4.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ******************************************************************************/

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
