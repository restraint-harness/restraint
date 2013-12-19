FILES := rhts-make.include

install:
	mkdir -p $(DESTDIR)/usr/share/rhts/lib
	for file in $(FILES); do install -m 755 $$file $(DESTDIR)/usr/share/rhts/lib; done
