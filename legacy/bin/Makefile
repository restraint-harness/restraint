FILES := rhts-environment.sh rhts-run-simple-test rhts-lint rhts-sync-block rhts-sync-set

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/bin
	for file in $(FILES); do install -m 755 $$file $(DESTDIR)/usr/bin; done
