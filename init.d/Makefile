
.PHONY: all
all:

.PHONY: install
install: restraintd
	install -m0755 -d $(DESTDIR)/etc/rc.d/init.d
	install -m0755 restraintd $(DESTDIR)/etc/rc.d/init.d

.PHONY: clean
clean:
