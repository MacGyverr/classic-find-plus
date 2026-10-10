# Independent GNU Make build; Meson/Ninja are not invoked.
.DEFAULT_GOAL := all
CC ?= cc
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
DESTDIR ?=
PYTHON ?= python3
CFLAGS ?= -O2 -g
SOURCES := $(shell cat sources.txt)
OBJECTS := $(patsubst src/%.c,build-make/%.o,$(SOURCES))
WORKER_OBJECTS := build-make/worker-main.o build-make/worker-read-only.o $(addprefix build-make/,content-search.o criteria.o date-expression.o search.o traversal.o metadata.o admin-protocol.o admin-client.o)
DEPS := $(OBJECTS:.o=.d) $(WORKER_OBJECTS:.o=.d) build-make/resources.d
GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk+-3.0 gio-2.0)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk+-3.0 gio-2.0)
GIO_CFLAGS = $(shell $(PKG_CONFIG) --cflags gio-2.0)
GIO_LIBS = $(shell $(PKG_CONFIG) --libs gio-2.0)
BASE_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_70 -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_70
export PREFIX DESTDIR
.PHONY: all check-deps install uninstall clean help force-config
all: build-make/classic-find-plus build-make/classic-find-plus-search-worker build-make/org.classicfind.Plus.Search.service
check-deps:
	@$(PKG_CONFIG) --exists 'gtk+-3.0 >= 3.22' 'glib-2.0 >= 2.70' gio-2.0 || { echo 'GTK3 >=3.22 and GLib/GIO >=2.70 development packages and pkg-config are required.' >&2; exit 1; }
	@command -v glib-compile-resources >/dev/null || { echo 'glib-compile-resources is required (GLib development tools).' >&2; exit 1; }
build-make:
	mkdir -p "$@"
build-make/config.h: VERSION scripts/build-config.awk force-config | build-make
	@awk -f scripts/build-config.awk VERSION > "$@.tmp"
	@cmp -s "$@.tmp" "$@" && rm "$@.tmp" || mv "$@.tmp" "$@"
build-make/org.classicfind.Plus.Search.service: VERSION scripts/build-config.awk force-config | build-make
	@awk -v mode=service -f scripts/build-config.awk VERSION > "$@.tmp"
	@cmp -s "$@.tmp" "$@" && rm "$@.tmp" || mv "$@.tmp" "$@"
build-make/resources.c: data/classic-find-plus.gresource.xml data/artwork/icon.png data/artwork/about.png | build-make check-deps
	glib-compile-resources --sourcedir=data --generate-source --c-name=classic_find_plus --target="$@" "$<"
build-make/%.o: src/%.c build-make/config.h | check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BASE_FLAGS) $(GTK_CFLAGS) -Iinclude -Ibuild-make -MMD -MP -c "$<" -o "$@"
build-make/resources.o: build-make/resources.c | check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BASE_FLAGS) -Wno-overlength-strings $(GTK_CFLAGS) -MMD -MP -c "$<" -o "$@"
build-make/worker-%.o: worker/%.c build-make/config.h | check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BASE_FLAGS) $(GIO_CFLAGS) -Iinclude -Iworker -Ibuild-make -MMD -MP -c "$<" -o "$@"
build-make/classic-find-plus-search-worker: $(WORKER_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o "$@" $(WORKER_OBJECTS) $(GIO_LIBS) $(LDLIBS)
build-make/classic-find-plus: $(OBJECTS) build-make/resources.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o "$@" $(OBJECTS) build-make/resources.o $(GTK_LIBS) $(LDLIBS)
install: all
	$(PYTHON) scripts/source-install.py install
uninstall:
	$(PYTHON) scripts/source-install.py uninstall
clean:
	rm -f $(OBJECTS) $(WORKER_OBJECTS) $(DEPS) build-make/config.h build-make/resources.c build-make/resources.o build-make/classic-find-plus build-make/classic-find-plus-search-worker build-make/org.classicfind.Plus.Search.service
	@rmdir build-make 2>/dev/null || :
help:
	@printf '%s\n' 'make [-jN] [CC=cc CFLAGS="-O2 -g"]' 'make install PREFIX=/usr/local [DESTDIR=/absolute/staging/path]' 'make install PREFIX="$$HOME/.local"' 'make uninstall PREFIX=... [DESTDIR=...] (tracked unchanged files only; refuses package ownership)' 'make clean (Make outputs only; preserves Meson builds/settings)' 'Install/uninstall requires Python 3; see docs/install.md.'
-include $(DEPS)
