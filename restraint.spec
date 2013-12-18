Name:		restraint
Version:	0.1.3
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+
URL:		https://github.com/p3ck/%{name}
Source0:	https://github.com/p3ck/%{name}/%{name}-%{version}.tar.gz

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)}
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
%if 0%{?rhel} == 4
%ifarch ppc64
export CFLAGS="-mminimal-toc"
%endif
%endif

pushd third-party
make
popd
pushd src
PKG_CONFIG_PATH=../third-party/tree/lib/pkgconfig make STATIC=1
popd


%install
%{__rm} -rf %{buildroot}

pushd third-party
make clean
popd
make DESTDIR=%{buildroot} install

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

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Dec 18 2013 Bill Peck <bpeck@redhat.com> 0.1.3-1
- Added localwatchdog plugins (bpeck@redhat.com)
- ignore pyc file from tito (bpeck@redhat.com)
- hack to tito build to add the third-party tarballs into the release.
  (bpeck@redhat.com)
- Updates to allow restraint to build from rhel4-rhel7 on all arches we care
  about. (bpeck@redhat.com)
- few more places to switch from guint64 to gulong. (bpeck@redhat.com)
- Use unsigned long. (bpeck@redhat.com)
- Add missing BuildRequies to spec (bpeck@redhat.com)
- link process.o into restraintd fix install to create dirs really simple spec
  file (bpeck@redhat.com)
- Make it easy to grab all the tarballs needed for static linking.
  (bpeck@redhat.com)

* Tue Dec 17 2013 Bill Peck <bpeck@redhat.com> 0.1.2-1
- new package built with tito


