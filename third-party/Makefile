
ZLIB = zlib-1.2.7
BZIP2 = bzip2-1.0.6
LIBFFI = libffi-3.0.11
GLIB = glib-2.34.3
LIBXML2 = libxml2-2.9.0
CURL = curl-7.29.0
LIBARCHIVE = libarchive-3.1.2
XZ = xz-5.0.4
LIBSOUP = libsoup-2.38.1

.PHONY: all
all: \
    tree/lib/pkgconfig/liblzma.pc \
    tree/lib/pkgconfig/zlib.pc \
    tree/lib/libbz2.a \
    tree/lib/pkgconfig/libffi.pc \
    tree/lib/pkgconfig/glib-2.0.pc \
    tree/lib/pkgconfig/libxml-2.0.pc \
    tree/lib/pkgconfig/libcurl.pc \
    tree/lib/pkgconfig/libarchive.pc \
    tree/lib/pkgconfig/libsoup-2.38.pc

tree/lib/pkgconfig/zlib.pc: $(ZLIB)
	( cd $(ZLIB) && ./configure --prefix=$(CURDIR)/tree --static )
	$(MAKE) -C $(ZLIB)
	$(MAKE) -C $(ZLIB) install

tree/lib/libbz2.a: $(BZIP2)
	$(MAKE) -C $(BZIP2)
	$(MAKE) -C $(BZIP2) PREFIX=$(CURDIR)/tree install

tree/lib/pkgconfig/liblzma.pc: $(XZ) $(XZ)/.patches-done
	( cd $(XZ) && ./configure --prefix=$(CURDIR)/tree \
	    --disable-lzma-links --disable-scripts \
	    --enable-static --disable-shared )
	$(MAKE) -C $(XZ)
	$(MAKE) -C $(XZ) install

tree/lib/pkgconfig/libffi.pc: $(LIBFFI)
	( cd $(LIBFFI) && ./configure --prefix=$(CURDIR)/tree --enable-portable-binary \
	    --enable-static --disable-shared )
	$(MAKE) -C $(LIBFFI)
	$(MAKE) -C $(LIBFFI) install

tree/lib/pkgconfig/glib-2.0.pc: $(GLIB) $(GLIB)/.patches-done tree/lib/pkgconfig/zlib.pc tree/lib/pkgconfig/libffi.pc
	( cd $(GLIB) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --disable-libelf --disable-fam --disable-xattr --disable-systemtap \
	    --disable-gcov --disable-dtrace --disable-gtk-doc --disable-gtk-doc-html \
	    --disable-man --disable-modular-tests \
	    --enable-static --disable-shared )
	$(MAKE) -C $(GLIB)
	$(MAKE) -C $(GLIB) install

tree/lib/pkgconfig/libxml-2.0.pc: $(LIBXML2) $(LIBXML2)/.patches-done tree/lib/pkgconfig/zlib.pc
	( cd $(LIBXML2) && ./configure --prefix=$(CURDIR)/tree \
	    --with-zlib=$(CURDIR)/tree --enable-static --disable-shared )
	$(MAKE) -C $(LIBXML2)
	$(MAKE) -C $(LIBXML2) install

tree/lib/pkgconfig/libsoup-2.38.pc: $(LIBSOUP) $(LIBSOUP)/.patches-done tree/lib/pkgconfig/glib-2.0.pc tree/lib/pkgconfig/libxml-2.0.pc
	( cd $(LIBSOUP) && PKG_CONFIG_PATH=../tree/lib/pkgconfig \
              ./configure --prefix=$(CURDIR)/tree \
	      --disable-glibtest --disable-tls-check --without-gnome --enable-static \
              --disable-shared )
	( PATH=$(CURDIR)/tree/bin:$(PATH) $(MAKE) -C $(LIBSOUP) )
	$(MAKE) -C $(LIBSOUP) install

tree/lib/pkgconfig/libcurl.pc: $(CURL) tree/lib/pkgconfig/zlib.pc
	( cd $(CURL) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --with-zlib=$(CURDIR)/tree --with-ssl --enable-ipv6 --disable-manual \
	    --enable-static --disable-shared )
	$(MAKE) -C $(CURL)
	$(MAKE) -C $(CURL) install

tree/lib/pkgconfig/libarchive.pc: $(LIBARCHIVE) $(LIBARCHIVE)/.patches-done \
				  tree/lib/pkgconfig/zlib.pc \
				  tree/lib/libbz2.a
	( cd $(LIBARCHIVE) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --disable-bsdtar --disable-bsdcpio --without-lzma \
	    --enable-static --disable-shared )
	$(MAKE) -C $(LIBARCHIVE)
	$(MAKE) -C $(LIBARCHIVE) install

$(LIBFFI).tar.gz:
	curl -O ftp://sourceware.org/pub/libffi/$@

$(GLIB).tar.xz:
	curl -O http://ftp.gnome.org/pub/gnome/sources/glib/2.34/$@

$(ZLIB).tar.gz:
	curl -O http://zlib.net/$@

$(BZIP2).tar.gz:
	curl -O http://www.bzip.org/1.0.6/$@

$(LIBXML2).tar.gz:
	curl -O ftp://xmlsoft.org/libxml2/$@

$(CURL).tar.bz2:
	curl -O http://curl.haxx.se/download/$@

$(LIBARCHIVE).tar.gz:
	curl -O http://libarchive.org/downloads/$@

$(XZ).tar.gz:
	curl -O http://tukaani.org/xz/$@

$(LIBSOUP).tar.xz:
	curl -O http://ftp.gnome.org/pub/GNOME/sources/libsoup/2.38/$@

%: %.tar.gz
	tar -xzf $<

%: %.tar.bz2
	tar -xjf $<

%: %.tar.xz tree/lib/pkgconfig/liblzma.pc
	$(CURDIR)/tree/bin/xzcat $< | tar -xf -

$(XZ)/.patches-done:
	patch -d$(XZ) -p1 <xz-no-pkgconfig-url.patch
	touch $@

$(GLIB)/.patches-done:
	patch -d$(GLIB) -p1 <glib-python2.2.patch
	patch -d$(GLIB) -p1 <glib-no-pkgconfig-version-check.patch
	patch -d$(GLIB) -p1 <glib-gcc3.patch
	touch $@

$(LIBXML2)/.patches-done:
	patch -d$(LIBXML2) -p1 <libxml2-timsort-gcc3.patch
	touch $@

$(LIBARCHIVE)/.patches-done:
	patch -d$(LIBARCHIVE) -p1 <libarchive-rhel3-headers.patch
	touch $@

$(LIBSOUP)/.patches-done:
	patch -d$(LIBSOUP) -p1 <libsoup-no-pkgconfig-version-check.patch
	touch $@

.PHONY: clean
clean:
	rm -rf $(XZ) $(ZLIB) $(BZIP2) $(LIBFFI) $(GLIB) $(LIBXML2) $(CURL) $(LIBARCHIVE) $(LIBSOUP) tree
