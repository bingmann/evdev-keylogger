#include <linux/input.h>

struct input_event_state {
	int altgr:1;
	int alt:1;
	int shift:1;
	int ctrl:1;
	int meta:1;
};
size_t translate_event(struct input_event *event, struct input_event_state *state, char *buffer, size_t buffer_len);
int load_system_keymap();
