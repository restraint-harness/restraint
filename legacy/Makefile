
SUBDIRS := bin lib

.PHONY: all
all:

.PHONY: clean
clean:

.PHONY: install
install:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i install; done

.PHONY: check
check:
