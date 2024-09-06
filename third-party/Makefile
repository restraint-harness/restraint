
ZLIB = zlib-1.2.13
BZIP2 = bzip2-1.0.8
LIBFFI = libffi-3.3
GLIB = glib-2.68.0
LIBXML2 = libxml2-2.9.10
CURL = curl-7.68.0
LIBARCHIVE = libarchive-3.4.0
XZ = xz-5.2.4
LIBSOUP = libsoup-2.52.2
OPENSSL = openssl-1.1.1w
SQLITE = sqlite-autoconf-3310100
INTLTOOL = intltool-0.51.0
LIBJSONC = json-c-0.16
AUTOCONF = autoconf-2.69
M4 = m4-1.4.19

arch := $(shell uname -m)

# glib2 requires python 2.7 or greater
# Find a python2.7 or python3 binary.
ifneq ("$(wildcard /usr/bin/python2.7)","")
    PYTHON := /usr/bin/python2.7
else
ifneq ("$(wildcard /usr/bin/python3)","")
    PYTHON := /usr/bin/python3
else
    # Take a wild stab in the dark.
    PYTHON := $(shell which python)
endif
endif

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
    tree/lib/pkgconfig/openssl.pc \
    tree/bin/autoheader \
    tree/bin/m4 \
    tree/lib/pkgconfig/libjson-c.pc

tree/bin/m4: $(M4) $(M4)/.patches-done
	( cd $(M4) && ./configure --prefix=$(CURDIR)/tree )
	$(MAKE) -C $(M4)
	$(MAKE) -C $(M4) install

tree/lib/pkgconfig/zlib.pc: $(ZLIB)
	( cd $(ZLIB) && ./configure --prefix=$(CURDIR)/tree --static )
	$(MAKE) -C $(ZLIB)
	$(MAKE) -C $(ZLIB) install

tree/bin/autoheader: tree/bin/m4 $(AUTOCONF) $(AUTOCONF)/.patches-done
	( cd $(AUTOCONF) && PATH=$(CURDIR)/tree/bin:$(PATH) ./configure --prefix=$(CURDIR)/tree )
	$(MAKE) -C $(AUTOCONF)
	$(MAKE) -C $(AUTOCONF) install

tree/lib/pkgconfig/libjson-c.pc: tree/bin/autoheader $(LIBJSONC)
	( mkdir -p $(LIBJSONC)/build && cd $(LIBJSONC)/build && cmake ../ -DCMAKE_INSTALL_PREFIX=$(CURDIR)/tree -DBUILD_SHARED_LIBS=OFF \
		-DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_LIBDIR=lib )
	( PATH=$(CURDIR)/tree/bin:$(PATH) $(MAKE) -C $(LIBJSONC)/build )
	$(MAKE) -C $(LIBJSONC)/build install

tree/lib/pkgconfig/openssl.pc: $(OPENSSL)
    ifeq ("$(arch)", "ppc64")
	( cd $(OPENSSL) && PATH=$(CURDIR)/tree/bin:$(PATH) ./Configure --prefix=$(CURDIR)/tree --libdir=$(CURDIR)/tree/lib -DPURIFY linux-ppc64)
    else
	( cd $(OPENSSL) && PATH=$(CURDIR)/tree/bin:$(PATH) ./config --prefix=$(CURDIR)/tree --libdir=$(CURDIR)/tree/lib -DPURIFY )
    endif
	$(MAKE) -C $(OPENSSL)
	$(MAKE) -C $(OPENSSL) PREFIX=$(CURDIR)/tree install_sw

tree/lib/libbz2.a: $(BZIP2) $(BZIP2)/.patches-done
	$(MAKE) -C $(BZIP2)
	$(MAKE) -C $(BZIP2) PREFIX=$(CURDIR)/tree install

tree/lib/pkgconfig/liblzma.pc: $(XZ)
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
	( PKG_CONFIG_PATH=$(CURDIR)/tree/lib/pkgconfig \
	    meson setup \
		--prefix=$(CURDIR)/tree --libdir=lib \
		--default-library=static \
		-Dtests=false -Dlibelf=disabled -Dxattr=false -Dlibmount=disabled -Dinternal_pcre=true \
	    $(CURDIR)/$(GLIB)/build $(CURDIR)/$(GLIB))
	meson install -C $(CURDIR)/$(GLIB)/build
#   this is a dirty hack to stop gcc reporting on warnings in the glib headers.
	sed -i -e 's/\-I/\-isystem/g' $(CURDIR)/tree/lib/pkgconfig/glib-2.0.pc

tree/lib/pkgconfig/libxml-2.0.pc: $(LIBXML2) $(LIBXML2)/.patches-done tree/lib/pkgconfig/zlib.pc
	( cd $(LIBXML2) && ./configure --prefix=$(CURDIR)/tree \
	    --with-zlib=$(CURDIR)/tree --without-python \
	    --enable-static --disable-shared )
	$(MAKE) -C $(LIBXML2)
	$(MAKE) -C $(LIBXML2) install

tree/lib/pkgconfig/sqlite3.pc: $(SQLITE)
	( cd $(SQLITE) && ./configure --prefix=$(CURDIR)/tree \
	    --enable-static --disable-shared )
	$(MAKE) -C $(SQLITE)
	$(MAKE) -C $(SQLITE) install

tree/bin/intltool-update: $(INTLTOOL) $(INTLTOOL)/.patches-done
	( cd $(INTLTOOL) && ./configure --prefix=$(CURDIR)/tree \
	    --enable-static --disable-shared )
	$(MAKE) -C $(INTLTOOL)
	$(MAKE) -C $(INTLTOOL) install

tree/lib/pkgconfig/libsoup-2.4.pc: $(LIBSOUP) $(LIBSOUP)/.patches-done tree/lib/pkgconfig/glib-2.0.pc tree/lib/pkgconfig/libxml-2.0.pc
	( cd $(LIBSOUP) && PATH=$(CURDIR)/tree/bin:$(PATH) PKG_CONFIG_PATH=../tree/lib/pkgconfig \
              ./configure --prefix=$(CURDIR)/tree \
	      --disable-glibtest --disable-tls-check --without-gnome --enable-static \
              --disable-shared --disable-vala)
	( PATH=$(CURDIR)/tree/bin:$(PATH) $(MAKE) -C $(LIBSOUP) )
	$(MAKE) -C $(LIBSOUP) install

tree/lib/pkgconfig/libcurl.pc: $(CURL) tree/lib/pkgconfig/zlib.pc tree/lib/pkgconfig/openssl.pc
	( cd $(CURL) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --with-zlib=$(CURDIR)/tree --with-ssl --enable-ipv6 --disable-manual \
	    --enable-static --disable-shared --disable-ldap --disable-ldaps --without-libidn --without-libssh2 \
	    --without-brotli --without-libpsl)
	$(MAKE) -C $(CURL)
	$(MAKE) -C $(CURL) install

tree/lib/pkgconfig/libarchive.pc: $(LIBARCHIVE) $(LIBARCHIVE)/.patches-done \
				  tree/lib/pkgconfig/zlib.pc \
				  tree/lib/libbz2.a \
				  tree/lib/pkgconfig/liblzma.pc
	( cd $(LIBARCHIVE) && PKG_CONFIG_PATH=../tree/lib/pkgconfig ./configure --prefix=$(CURDIR)/tree \
	    --disable-bsdtar --disable-bsdcpio \
	    --without-nettle --disable-acl \
	    --enable-static --disable-shared --without-zstd )
	$(MAKE) -C $(LIBARCHIVE)
	$(MAKE) -C $(LIBARCHIVE) install

$(BZIP2)/.patches-done: $(BZIP2)
	patch -d$(BZIP2) -p1 <bzip2-add-fPIE.patch
	touch $@

$(LIBARCHIVE)/.patches-done: $(LIBARCHIVE)
	patch -d$(LIBARCHIVE) -p1 <libarchive-use-pkgconfig.patch
	touch $@

TAR_BALLS = $(LIBFFI).tar.gz
$(LIBFFI).tar.gz:
	curl -f -L -O http://mirrors.kernel.org/sourceware/libffi/$@

TAR_BALLS += $(GLIB).tar.xz
$(GLIB).tar.xz:
	curl -f -L -O https://ftp.gnome.org/pub/gnome/sources/glib/2.68/$@

TAR_BALLS += $(ZLIB).tar.gz
$(ZLIB).tar.gz:
	curl -f -L -O https://zlib.net/fossils/$@

TAR_BALLS += $(BZIP2).tar.gz
$(BZIP2).tar.gz:
	curl -f -L -O https://www.sourceware.org/pub/bzip2/$@

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
	curl -f -L -O https://sqlite.org/2020/$@

TAR_BALLS += $(INTLTOOL).tar.gz
$(INTLTOOL).tar.gz:
	curl -f -L -O https://launchpad.net/intltool/trunk/0.51.0/+download/$@

TAR_BALLS += $(LIBSOUP).tar.xz
$(LIBSOUP).tar.xz:
	curl -f -L -O http://ftp.gnome.org/pub/GNOME/sources/libsoup/2.52/$@

TAR_BALLS += $(AUTOCONF).tar.gz
$(AUTOCONF).tar.gz:
	curl -f -L -O http://ftp.gnu.org/gnu/autoconf/$@

TAR_BALLS += $(LIBJSONC).tar.gz
$(LIBJSONC).tar.gz:
	curl -f -L -O https://s3.amazonaws.com/json-c_releases/releases/$@

TAR_BALLS += $(OPENSSL).tar.gz
$(OPENSSL).tar.gz:
	curl -f -L -O https://www.openssl.org/source/$@

TAR_BALLS += $(M4).tar.xz
$(M4).tar.xz:
	curl -f -L -O https://ftp.gnu.org/gnu/m4/$@

tarballs: $(TAR_BALLS)
	printf '%s\n' $(TAR_BALLS) >$@

%: %.tar.gz
	tar -xzf $<

%: %.tar.bz2
	tar -xjf $<

%: %.tar.xz tree/lib/pkgconfig/liblzma.pc
	$(CURDIR)/tree/bin/xzcat $< | tar -xf -

$(LIBFFI)/.patches-done: $(LIBFFI)
	touch $@

$(GLIB)/.patches-done: $(GLIB)
	if [ -e glib_new_close_range_arg.patch ]; then\
		patch -d$(GLIB) -p1 < glib_new_close_range_arg.patch;\
	fi
	touch $@

$(INTLTOOL)/.patches-done: $(INTLTOOL)
	patch -d$(INTLTOOL) -p1 <intltool-perl5.26-regex-fixes.patch
	touch $@

$(LIBSOUP)/.patches-done: $(LIBSOUP)
	patch -d$(LIBSOUP) -p1 <libsoup-no-pkgconfig-version-check.patch
	patch -d$(LIBSOUP) -p1 <libsoup-perl580.patch
	patch -d$(LIBSOUP) -p1 <libsoup-gcc15-incompatible-pointers.patch
	sed -i -e "/^#!/c\#!$(PYTHON)" $(LIBSOUP)/libsoup/tld-parser.py
	touch $@

$(LIBXML2)/.patches-done: $(LIBXML2)
	patch -d$(LIBXML2) -p1 <libxml2_add_ppc64le.patch

$(AUTOCONF)/.patches-done: $(AUTOCONF)
	patch -d$(AUTOCONF) -p1 <autoconf-ppc64le.patch
	touch $@

$(M4)/.patches-done: $(M4)
	patch -d$(M4) -p1 < m4-1.4.19-fortify-source.patch
	patch -d$(M4) -p1 < m4-1.4.19-nodiscard-gcc15.patch
	touch $@

.PHONY: clean
clean:
	rm -rf $(XZ) $(ZLIB) $(BZIP2) $(LIBFFI) $(GLIB) $(LIBXML2) $(CURL) $(LIBARCHIVE) $(LIBSOUP) $(SQLITE) $(INTLTOOL) $(CMAKE) $(OPENSSL) $(AUTOCONF) $(LIBJSONC) $(M4) tree

clean-tarballs:
	rm -rf $(TAR_BALLS)
