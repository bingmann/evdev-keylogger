CFLAGS ?= -O3 -march=native -fomit-frame-pointer -W -Wall -pipe

logger: logger.c keymap.c keymap.h process.c process.h
clean:
	rm -f logger
.PHONY: clean
