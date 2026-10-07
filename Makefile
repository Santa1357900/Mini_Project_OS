CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
all: psearch

psearch: psearch.c
	$(CC) $(CFLAGS) -o $@ $<

test: psearch
	sh tests/test.sh

clean:
	rm -f psearch
