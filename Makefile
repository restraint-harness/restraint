
SUBDIRS := src init.d plugins legacy scripts

.PHONY: all
all:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i; done

.PHONY: install
install:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i install; done

.PHONY: check
check:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i check; done

.PHONY: valgrind
valgrind:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i valgrind; done

.PHONY: clean
clean:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
