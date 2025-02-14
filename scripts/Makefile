FILES := rstrnt-backup rstrnt-restore rstrnt-prepare-reboot rstrnt-reboot check_beaker rstrnt-package rstrnt-sync-set rstrnt-sync-block

.PHONY: all
all:

.PHONY: clean
clean:

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/bin
	for file in $(FILES); do install -m 755 $$file $(DESTDIR)/usr/bin; done

	#The check_beaker script when executed, outputs a config file to
        #/etc/restraint. Create the directory for packaging purposes.
	mkdir -p $(DESTDIR)/etc/restraint

.PHONY: check
check:
