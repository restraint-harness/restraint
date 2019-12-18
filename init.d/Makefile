
.PHONY: all
all:

.PHONY: install
install: restraintd
	install -m0755 -d $(DESTDIR)/etc/init.d
	install -m0755 restraintd $(DESTDIR)/etc/init.d
	install -m0755 -d $(DESTDIR)/usr/lib/systemd/system
	install -m0644 restraintd.service $(DESTDIR)/usr/lib/systemd/system

.PHONY: clean
clean:

.PHONY: check
check:
