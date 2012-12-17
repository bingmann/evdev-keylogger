#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "evdev.h"

int find_default_keyboard(char *buffer, size_t buffer_len)
{
	FILE *devices;
	char events[128];
	char handlers[128];
	char *event;
	int i;

	devices = fopen("/proc/bus/input/devices", "r");
	if (!devices) {
		perror("fopen");
		return -1;
	}
	while (fgets(events, sizeof(events), devices)) {
		if (strstr(events, "H: Handlers=") == events)
			strcpy(handlers, events);
		else if (!strcmp(events, "B: EV=120013\n") && (event = strstr(handlers, "event"))) {
			for (i = 0, event += sizeof("event") - 1; *event && isdigit(*event); ++event, ++i)
				handlers[i] = *event;
			handlers[i] = '\0';
			fclose(devices);
			return snprintf(buffer, buffer_len, "/dev/input/event%s", handlers);
		}
	}
	fclose(devices);
	return -1;
}
