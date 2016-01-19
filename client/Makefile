
.PHONY: all
all:

.PHONY: install
install:
	install -m0755 -d $(DESTDIR)/usr/share/restraint/client
	install -m0644 job2html.xml $(DESTDIR)/usr/share/restraint/client
	install -m0644 job2junit.xml $(DESTDIR)/usr/share/restraint/client
	install -m0644 job_schema.rng $(DESTDIR)/usr/share/restraint/client
	install -m0755 -d $(DESTDIR)/usr/share/restraint/client/bootstrap
	install -m0644 bootstrap/* $(DESTDIR)/usr/share/restraint/client/bootstrap

.PHONY: clean
clean:

.PHONY: check
check:
