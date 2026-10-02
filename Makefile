# dew — terminal to-do list with waves and a Game of Life pane.

VERSION := 0.1.0
PREFIX  ?= $(HOME)/.local
CC      ?= cc
PKG     ?= pkg-config
CFLAGS  ?= -O2

# ncursesw's flags carry the feature macros (_DEFAULT_SOURCE, _XOPEN_SOURCE),
# so every object uses them; defining _XOPEN_SOURCE ourselves would clash.
NCURSES_CFLAGS := $(shell $(PKG) --cflags ncursesw)
NCURSES_LIBS   := $(shell $(PKG) --libs ncursesw)
DEW_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc \
              -DDEW_VERSION='"$(VERSION)"' $(NCURSES_CFLAGS)

CORE := util tasks
UI   :=

CORE_OBJ := $(CORE:%=build/%.o)
UI_OBJ   := $(UI:%=build/%.o)
TEST_OBJ := $(patsubst tests/%.c,build/tests/%.o,$(wildcard tests/*.c))
HEADERS  := $(wildcard src/*.h)

all: dew

dew: $(CORE_OBJ) $(UI_OBJ) build/main.o
	$(CC) $(LDFLAGS) -o $@ $^ $(NCURSES_LIBS) -lm

build/%.o: src/%.c $(HEADERS) | build
	$(CC) $(DEW_CFLAGS) $(CFLAGS) -c -o $@ $<

build/tests/%.o: tests/%.c tests/test.h $(HEADERS) | build
	$(CC) $(DEW_CFLAGS) $(CFLAGS) -c -o $@ $<

build/dew_tests: $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

build:
	mkdir -p build/tests

test: build/dew_tests
	./build/dew_tests $(SUITE)

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	        LDFLAGS="-fsanitize=address,undefined" all test

install: dew
	install -Dm755 dew $(DESTDIR)$(PREFIX)/bin/dew
	install -Dm644 docs/dew.1 $(DESTDIR)$(PREFIX)/share/man/man1/dew.1
	install -Dm644 docs/tldr/dew.md $(DESTDIR)$(HOME)/.config/tldr/pages/common/dew.md

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dew $(DESTDIR)$(PREFIX)/share/man/man1/dew.1 \
	      $(DESTDIR)$(HOME)/.config/tldr/pages/common/dew.md

clean:
	rm -rf build dew

.PHONY: all test debug install uninstall clean
