
CFLAGS ?= -O -g

PACKAGES = glib-2.0 libxml-2.0
CFLAGS += -Wall -std=c99 $(shell pkg-config --cflags $(PACKAGES))
ifeq ($(STATIC),1)
    LIBS = -Wl,-Bstatic $(shell pkg-config --libs $(PACKAGES)) -Wl,-Bdynamic -pthread -lrt
else
    LIBS = $(shell pkg-config --libs $(PACKAGES))
endif

hello: hello.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBS)
