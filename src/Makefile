
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
    LIBS = -Wl,-Bstatic -Wl,-\( $(shell pkg-config --libs $(PACKAGES)) -lgmodule-2.0 -llzma -lbz2 -lz -lffi -lssl -lcrypto -lselinux -Wl,-\) -Wl,-Bdynamic -lm -pthread -lrt -lresolv -ldl -lutil $(LFLAGS)
else
    LIBS = $(shell pkg-config --libs $(PACKAGES) $(XTRAPKGS)) -lutil
endif

.PHONY: all
all: restraint restraintd rstrnt-report-result rstrnt-report-log rstrnt-adjust-watchdog

rstrnt-report-result: cmd_result.o upload.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

rstrnt-report-log: cmd_log.o upload.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

rstrnt-adjust-watchdog: cmd_watchdog.o utils.o errors.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

restraint: client.o
	$(CC) $(LDFLAFS) -o $@ $^ $(LIBS)

restraintd: server.o recipe.o task.o packages.o fetch_git_task.o fetch_http_task.o param.o role.o metadata.o process.o message.o dependency.o utils.o config.o errors.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

fetch_git_task.o: task.h
fetch_http_task.o: task.h
packages.o: packages.h
task.o: task.h
recipe.o: recipe.h task.h
param.o: param.h
role.o: role.h
server.o: recipe.h task.h server.h
expect_http.o: expect_http.h
role.o: role.h
client.o: client.h
process.o: process.h
message.o: message.h
dependency.o: dependency.h
utils.o: utils.h
config.o: config.h
errors.o: errors.h

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

.PHONY: install
install: all
	install -m0755 -d $(DESTDIR)/usr/bin
	install -m0755 restraint $(DESTDIR)/usr/bin
	install -m0755 restraintd $(DESTDIR)/usr/bin
	install -m0755 rstrnt-report-result $(DESTDIR)/usr/bin
	install -m0755 rstrnt-report-log $(DESTDIR)/usr/bin
	install -m0755 rstrnt-adjust-watchdog $(DESTDIR)/usr/bin

.PHONY: clean
clean:
	rm -f restraint restraintd rstrnt-report-result \
              rstrnt-report-log rstrnt-adjust-watchdog $(TEST_PROGS) *.o
