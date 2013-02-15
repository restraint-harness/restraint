
GTESTER ?= gtester
CC = gcc
CFLAGS ?= -O -g

PACKAGES = glib-2.0 gobject-2.0 gio-2.0 libxml-2.0 libsoup-2.4 libarchive
CFLAGS += -Wall -std=c99 $(shell pkg-config --cflags $(PACKAGES))
ifeq ($(STATIC),1)
    LIBS = -Wl,-Bstatic $(shell pkg-config --libs $(PACKAGES)) -Wl,-Bdynamic -pthread -lrt
else
    LIBS = $(shell pkg-config --libs $(PACKAGES))
endif

.PHONY: all
all: restraint

restraint: main.o recipe.o fetch_git_task.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

main.o: recipe.h
recipe.o: recipe.h
fetch_git_task.o: fetch_git_task.h

TEST_PROGS =
test_%: test_%.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEST_PROGS += test_fetch_task
test_fetch_task: fetch_task.o
test_fetch_task.o: fetch_task.h

TEST_PROGS += test_recipe
test_recipe: recipe.o
test_recipe.o: recipe.h

.PHONY: check
check: $(TEST_PROGS)
	MALLOC_CHECK_=2 \
	G_DEBUG="fatal_warnings fatal_criticals" \
	G_SLICE="debug-blocks" \
	$(GTESTER) --verbose $^

.PHONY: clean
clean:
	rm -f restraint $(TEST_PROGS) *.o
