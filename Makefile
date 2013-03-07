
GTESTER ?= gtester
CC = gcc
CFLAGS ?= -O -g

PACKAGES = glib-2.0 gobject-2.0 gio-2.0 libxml-2.0 libsoup-2.4
CFLAGS += -Wall -Werror -std=c99 $(shell pkg-config --cflags $(PACKAGES))
ifeq ($(STATIC),1)
    LIBS = -Wl,-Bstatic $(shell pkg-config --libs $(PACKAGES)) -Wl,-Bdynamic -pthread -lrt
else
    LIBS = $(shell pkg-config --libs $(PACKAGES))
endif

.PHONY: all
all: restraint

restraint: main.o recipe.o task.o packages.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

packages.o: packages.h
task.o: task.h
recipe.o: recipe.h task.h
main.o: recipe.h task.h

TEST_PROGS =
test_%: test_%.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEST_PROGS += test_packages
test_packages: packages.o
test_packages.o: packages.h

TEST_PROGS += test_task
test_task: task.o packages.o
test_task.o: task.h

TEST_PROGS += test_recipe
test_recipe: recipe.o task.o packages.o
test_recipe.o: recipe.h task.h

.PHONY: check
check: $(TEST_PROGS)
	PATH="$(CURDIR)/test-dummies:$$PATH" \
	MALLOC_CHECK_=2 \
	G_DEBUG="fatal_warnings fatal_criticals" \
	G_SLICE="debug-blocks" \
	$(GTESTER) --verbose $^

.PHONY: clean
clean:
	rm -f restraint $(TEST_PROGS) *.o
