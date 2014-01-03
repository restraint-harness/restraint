FILES := rhts-environment.sh rhts-reboot rhts-run-simple-test

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/bin
	for file in $(FILES); do install -m 755 $$file $(DESTDIR)/usr/bin; done
