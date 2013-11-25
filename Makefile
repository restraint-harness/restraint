
CC = gcc
#CFLAGS ?= -O -g
CFLAGS ?= -g

PACKAGES = glib-2.0 gobject-2.0 gio-2.0 gio-unix-2.0 libxml-2.0 libsoup-2.4 libarchive
CFLAGS += -Wall -Werror -std=c99 $(shell pkg-config --cflags $(PACKAGES))
ifeq ($(STATIC),1)
    # The right thing to do here is `pkg-config --libs --static`, which would 
    # include Libs.private in the link command.
    # But really old pkg-config versions don't understand that so let's just 
    # hardcode the "private" libs here.
    # The -( -) grouping means we don't have to worry about getting all the 
    # dependent libs in the right order (normally pkg-config would do that for 
    # us).
    LIBS = -Wl,-Bstatic -Wl,-\( $(shell pkg-config --libs $(PACKAGES)) -lgmodule-2.0 -llzma -lbz2 -lz -lffi -lssl -lm -lcrypto -lselinux -Wl,-\) -Wl,-Bdynamic -pthread -lrt -lresolv -ldl -lutil $(LFLAGS)
else
    LIBS = $(shell pkg-config --libs $(PACKAGES) $(XTRAPKGS)) -lutil
endif

.PHONY: all
all: restraint restraintd report_result rhts-submit-log

report_result: result.o upload.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

rhts-submit-log: result_log.o upload.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

restraint: client.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

restraintd: server.o recipe.o task.o packages.o fetch_git_task.o fetch_http_task.o param.o role.o metadata.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

fetch_git_task.o: task.h
fetch_http_task.o: task.h
packages.o: packages.h
task.o: task.h
recipe.o: recipe.h task.h
param.o: param.h
role.o: role.h
server.o: recipe.h task.h
expect_http.o: expect_http.h
role.o: role.h

TEST_PROGS =
test_%: test_%.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEST_PROGS += test_packages
test_packages: packages.o
test_packages.o: packages.h

TEST_PROGS += test_task
test_task: task.o packages.o fetch_git_task.o expect_http.o param.o role.o metadata.o
test_task.o: task.h expect_http.h

test-data/git-remote: test-data/git-remote.tgz
	tar -C test-data -xzf $<

TEST_PROGS += test_recipe
test_recipe: recipe.o task.o packages.o fetch_git_task.o param.o role.o metadata.o
test_recipe.o: recipe.h task.h param.h

TEST_PROGS += test_metadata
test_metadata: metadata.o task.o fetch_git_task.o param.o role.o packages.o
test_metadata.o: task.h

.PHONY: check
check: $(TEST_PROGS) test-data/git-remote
	./run-tests.sh $(TEST_PROGS)

.PHONY: valgrind
valgrind: $(TEST_PROGS) test-data/git-remote
	./run-tests.sh --valgrind $(TEST_PROGS)

.PHONY: clean
clean:
	rm -f restraint restraintd report_result $(TEST_PROGS) *.o
