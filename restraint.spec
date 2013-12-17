Name:		restraint
Version:	0.1.2
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+
URL:		https://github.com/p3ck/%{name}
Source0:	https://github.com/p3ck/%{name}/%{name}-%{version}.tar.gz

BuildRequires:	pkgconfig
BuildRequires:	gettext
BuildRequires:	perl-XML-Parser
BuildRequires:	openssl-devel
BuildRequires:	libselinux-devel
BuildRequires:	glibc-devel
#if not static build
BuildRequires:	zlib-devel
# If static build...
%if 0%{?rhel} >= 6
BuildRequires:	libselinux-static
BuildRequires:	openssl-static
BuildRequires:	glibc-static
%endif

%description
restraint harness which can run standalone or with beaker.  when provided a recipe xml it will execute
each task listed in the recipe until done.

%prep
%setup -q


%build
%ifarch i386
export CFLAGS="-march=i486"
%endif

pushd third-party && make
popd
pushd src && PKG_CONFIG_PATH=../third-party/tree/lib/pkgconfig make STATIC=1
popd


%install
pushd third-party && make clean
popd
DESTDIR=$RPM_BUILD_ROOT make install

%files
%defattr(-,root,root,-)
/etc/rc.d/init.d/restraintd
/usr/bin/report_result
/usr/bin/%{name}
/usr/bin/%{name}d
/usr/bin/rhts-environment.sh
/usr/bin/rhts-reboot
/usr/bin/rhts-submit-log
/usr/share/restraint
/usr/share/restraint/plugins/run_plugins
/usr/share/restraint/plugins/localwatchdog
/usr/share/restraint/plugins/report_result/01_dmesg_check

%doc


%changelog
* Tue Dec 17 2013 Bill Peck <bpeck@redhat.com> 0.1.2-1
- new package built with tito


