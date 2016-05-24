
ZLIB = zlib-1.2.8
BZIP2 = bzip2-1.0.6
LIBFFI = libffi-3.1
GLIB = glib-2.38.0
LIBXML2 = libxml2-2.9.1
CURL = curl-7.29.0
LIBARCHIVE = libarchive-3.2.1
XZ = xz-5.2.2
LIBSOUP = libsoup-2.48.1
CMAKE = cmake-3.2.3
LIBSSH = libssh-0.7.3
OPENSSL = openssl-1.0.1m
SQLITE = sqlite-autoconf-3080002
INTLTOOL = intltool-0.35.5

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
    tree/lib/pkgconfig/sqlite3.pc \
    tree/bin/intltool-update \
    tree/lib/pkgconfig/libsoup-2.4.pc \
    tree/bin/cmake \
    tree/lib/pkgconfig/openssl.pc \
    tree/lib/pkgconfig/libssh.pc 

tree/lib/pkgconfig/zlib.pc: $(ZLIB)
	( cd $(ZLIB) && ./configure --prefix=$(CURDIR)/tree --static )
	$(MAKE) -C $(ZLIB)
	$(MAKE) -C $(ZLIB) install

tree/bin/cmake: $(CMAKE)
	( cd $(CMAKE) && ./bootstrap --prefix=$(CURDIR)/tree )
	$(MAKE) -C $(CMAKE)
	$(MAKE) -C $(CMAKE) PREFIX=$(CURDIR)/tree install
	touch $@

tree/lib/pkgconfig/libssh.pc: $(LIBSSH) $(LIBSSH)/.patches-done tree/bin/cmake tree/lib/pkgconfig/openssl.pc tree/lib/pkgconfig/zlib.pc
	( mkdir -p $(LIBSSH)/build && cd $(LIBSSH)/build && PATH=$(CURDIR)/tree/bin:$(CURDIR)/tree:$(PATH) PKG_CONFIG_PATH=../tree/lib/pkgconfig cmake -DCMAKE_INSTALL_PREFIX=$(CURDIR)/tree -DWITH_STATIC_LIB=ON -DWITHOUT_SHARED_LIB=ON -DWITH_GSSAPI=OFF -DOPENSSL_ROOT_DIR=$(CURDIR)/tree -DZLIB_ROOT_DIR=$(CURDIR)/tree .. )
	$(MAKE) -C $(LIBSSH)/build
	$(MAKE) -C $(LIBSSH)/build install

tree/lib/pkgconfig/openssl.pc: $(OPENSSL)
	( cd $(OPENSSL) && ./config --prefix=$(CURDIR)/tree -DPURIFY )
	$(MAKE) -C $(OPENSSL)
	$(MAKE) -C $(OPENSSL) PREFIX=$(CURDIR)/tree install_sw

tree/lib/libbz2.a: $(BZIP2)
	$(MAKE) -C $(BZIP2)
	$(MAKE) -C $(BZIP2) PREFIX=$(CURDIR)/tree install

tree/lib/pkgconfig/liblzma.pc: $(XZ) $(XZ)/.patches-done
	( cd $(XZ) && ./configure --prefix=$(CURDIR)/tree \
	    --disable-lzma-links --disable-scripts \
	    --enable-static --disable-shared )
	$(MAKE) -C $(XZ)
	$(MAKE) -C $(XZ) install

tree/lib/pkgconfig/libffi.pc: $(LIBFFI) $(LIBFFI)/.patches-done
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
	    --with-zlib=$(CURDIR)/tree --without-python \
	    --enable-static --disable-shared )
	$(MAKE) -C $(LIBXML2)
	$(MAKE) -C $(LIBXML2) install

tree/lib/pkgconfig/sqlite3.pc: $(SQLITE) $(SQLITE)/.patches-done
	( cd $(SQLITE) && ./configure --prefix=$(CURDIR)/tree \
	    --enable-static --disable-shared )
	$(MAKE) -C $(SQLITE)
	$(MAKE) -C $(SQLITE) install

tree/bin/intltool-update: $(INTLTOOL)
	( cd $(INTLTOOL) && ./configure --prefix=$(CURDIR)/tree \
	    --enable-static --disable-shared )
	$(MAKE) -C $(INTLTOOL)
	$(MAKE) -C $(INTLTOOL) install

tree/lib/pkgconfig/libsoup-2.4.pc: $(LIBSOUP) $(LIBSOUP)/.patches-done tree/lib/pkgconfig/glib-2.0.pc tree/lib/pkgconfig/libxml-2.0.pc
tree/lib/pkgconfig/libsoup-2.4.pc: $(LIBSOUP) $(LIBSOUP)/.patches-done tree/lib/pkgconfig/glib-2.0.pc tree/lib/pkgconfig/libxml-2.0.pc
	( cd $(LIBSOUP) && PATH=$(CURDIR)/tree/bin:$(PATH) PKG_CONFIG_PATH=../tree/lib/pkgconfig \
              ./configure --prefix=$(CURDIR)/tree \
	      --disable-glibtest --disable-tls-check --without-gnome --enable-static \
              --disable-shared )
	( PATH=$(CURDIR)/tree/bin:$(PATH) $(MAKE) -C $(LIBSOUP) )
	$(MAKE) -C $(LIBSOUP) install

tree/lib/pkgconfig/libcurl.pc: $(CURL) $(CURL)/.patches-done tree/lib/pkgconfig/zlib.pc tree/lib/pkgconfig/openssl.pc
	( cd $(CURL) && LIBS="-ldl" PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --with-zlib=$(CURDIR)/tree --with-ssl=$(CURDIR)/tree --enable-ipv6 --disable-manual \
	    --enable-static --disable-shared --disable-ldap --disable-ldaps --without-libidn)
	$(MAKE) -C $(CURL)
	$(MAKE) -C $(CURL) install

tree/lib/pkgconfig/libarchive.pc: $(LIBARCHIVE) $(LIBARCHIVE)/.patches-done \
				  tree/lib/pkgconfig/zlib.pc \
				  tree/lib/libbz2.a \
				  tree/lib/pkgconfig/liblzma.pc
	( cd $(LIBARCHIVE) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --disable-bsdtar --disable-bsdcpio \
	    --without-nettle --disable-acl \
	    --enable-static --disable-shared )
	$(MAKE) -C $(LIBARCHIVE)
	$(MAKE) -C $(LIBARCHIVE) install

TAR_BALLS = $(LIBFFI).tar.gz
$(LIBFFI).tar.gz:
	curl -f -L -O http://mirrors.kernel.org/sourceware/libffi/$@

TAR_BALLS += $(GLIB).tar.xz
$(GLIB).tar.xz:
	curl -f -L -O http://ftp.gnome.org/pub/gnome/sources/glib/2.38/$@

TAR_BALLS += $(ZLIB).tar.gz
$(ZLIB).tar.gz:
	curl -f -L -O http://zlib.net/$@

TAR_BALLS += $(BZIP2).tar.gz
$(BZIP2).tar.gz:
	curl -f -L -O http://www.bzip.org/1.0.6/$@

TAR_BALLS += $(LIBXML2).tar.gz
$(LIBXML2).tar.gz:
	curl -f -L -O http://xmlsoft.org/sources/$@

TAR_BALLS += $(CURL).tar.bz2
$(CURL).tar.bz2:
	curl -f -L -O https://curl.haxx.se/download/$@

TAR_BALLS += $(LIBARCHIVE).tar.gz
$(LIBARCHIVE).tar.gz:
	curl -f -L -O http://libarchive.org/downloads/$@

TAR_BALLS += $(XZ).tar.gz
$(XZ).tar.gz:
	curl -f -L -O http://downloads.sourceforge.net/project/lzmautils/$@

TAR_BALLS += $(SQLITE).tar.gz
$(SQLITE).tar.gz:
	curl -f -L -O http://sqlite.org/2013/$@

TAR_BALLS += $(INTLTOOL).tar.gz
$(INTLTOOL).tar.gz:
	curl -f -L -O http://ftp.gnome.org/pub/gnome/sources/intltool/0.35/$@

TAR_BALLS += $(LIBSOUP).tar.xz
$(LIBSOUP).tar.xz:
	curl -f -L -O http://ftp.gnome.org/pub/GNOME/sources/libsoup/2.48/$@

TAR_BALLS += $(LIBSSH).tar.xz
$(LIBSSH).tar.xz:
	curl -f -L -O https://git.libssh.org/projects/libssh.git/snapshot/$@

TAR_BALLS += $(OPENSSL).tar.gz
$(OPENSSL).tar.gz:
	curl -f -L -O https://www.openssl.org/source/$@

TAR_BALLS += $(CMAKE).tar.gz
$(CMAKE).tar.gz:
	curl -f -L -O http://www.cmake.org/files/v3.2/$@

tarballs: $(TAR_BALLS)
	printf '%s\n' $(TAR_BALLS) >$@

%: %.tar.gz
	tar -xzf $<

%: %.tar.bz2
	tar -xjf $<

%: %.tar.xz tree/lib/pkgconfig/liblzma.pc
	$(CURDIR)/tree/bin/xzcat $< | tar -xf -

$(CURL)/.patches-done: $(CURL)
	patch -d$(CURL) -p1 <curl_add_ppc64le.patch
	patch -d$(CURL) -p1 <curl_pkgconfig.patch
	touch $@

$(XZ)/.patches-done: $(XZ)
	patch -d$(XZ) -p1 <xz-no-pkgconfig-url.patch
	patch -d$(XZ) -p1 <xz_add_ppc64le.patch
	touch $@

$(LIBFFI)/.patches-done: $(LIBFFI)
	patch -d$(LIBFFI) -p1 <libffi-3.1-toolexeclibdir.patch
	touch $@

$(SQLITE)/.patches-done: $(SQLITE)
	patch -d$(SQLITE) -p1 <sqlite_add_ppc64le.patch
	touch $@

$(GLIB)/.patches-done: $(GLIB)
	patch -d$(GLIB) -p1 <glib-python2.2.patch
	patch -d$(GLIB) -p1 <glib-no-pkgconfig-version-check.patch
	patch -d$(GLIB) -p1 <glib-g_socket_recovery.patch
#	patch -d$(GLIB) -p1 <glib-gcc3.patch
	patch -d$(GLIB) -p1 <glib-format-nonliteral.patch
	touch $@

$(LIBARCHIVE)/.patches-done: $(LIBARCHIVE)
#	patch -d$(LIBARCHIVE) -p1 <libarchive-rhel3-headers.patch
	patch -d$(LIBARCHIVE) -p1 <libarchive-use-pkgconfig.patch
	patch -d$(LIBARCHIVE) -p1 <libarchive_add_ppc64le.patch
	touch $@

$(LIBSOUP)/.patches-done: $(LIBSOUP)
	patch -d$(LIBSOUP) -p1 <libsoup-no-pkgconfig-version-check.patch
	patch -d$(LIBSOUP) -p1 <libsoup-perl580.patch
	touch $@

$(LIBXML2)/.patches-done: $(LIBXML2)
	patch -d$(LIBXML2) -p1 <libxml2_add_ppc64le.patch
	touch $@

$(LIBSSH)/.patches-done: $(LIBSSH)
	patch -d$(LIBSSH) -p1 <libssh_noshared.patch
	patch -d$(LIBSSH) -p1 <libssh_malformed_pubkey.patch
	touch $@

.PHONY: clean
clean:
	rm -rf $(XZ) $(ZLIB) $(BZIP2) $(LIBFFI) $(GLIB) $(LIBXML2) $(CURL) $(LIBARCHIVE) $(LIBSOUP) $(SQLITE) $(INTLTOOL) $(CMAKE) $(LIBSSH) $(OPENSSL) tree
