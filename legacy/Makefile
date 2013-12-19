
SUBDIRS := bin lib

.PHONY: all
all:

.PHONY: install
install:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
