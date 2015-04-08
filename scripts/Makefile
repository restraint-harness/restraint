FILES := rstrnt-backup rstrnt-restore rstrnt-reboot check_beaker rstrnt-package

.PHONY: all
all:

.PHONY: clean
clean:

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/bin
	for file in $(FILES); do install -m 755 $$file $(DESTDIR)/usr/bin; done

.PHONY: check
check:
