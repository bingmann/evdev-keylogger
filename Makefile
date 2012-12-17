CFLAGS ?= -O3 -march=native -fomit-frame-pointer -pipe

logger: logger.c keymap.c keymap.h evdev.c evdev.h process.c process.h
clean:
	rm -f logger
.PHONY: clean
