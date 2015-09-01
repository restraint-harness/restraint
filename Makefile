
SUBDIRS := src init.d plugins legacy scripts client

.PHONY: all
all:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i; done

.PHONY: install
install:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
	install -m0755 -d $(DESTDIR)/var/lib/restraint

.PHONY: check
check:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i check; done

.PHONY: valgrind
valgrind:
	set -e; $(MAKE) -C src valgrind

.PHONY: clean
clean:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
