Name:		restraint
Version:	0.1.3
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+
URL:		https://github.com/p3ck/%{name}
Source0:	https://github.com/p3ck/%{name}/%{name}-%{version}.tar.gz

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:	pkgconfig
BuildRequires:	gettext
BuildRequires:	perl-XML-Parser
BuildRequires:	openssl-devel
BuildRequires:	libselinux-devel
BuildRequires:	glibc-devel
%if 0%{?rhel}%{?fedora} > 4
BuildRequires: selinux-policy-devel
%endif

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

%package rhts
Summary:	Allow unmodified rhts tests to run under restraint
Group:		Applications/Internet
Requires:	restraint = %{version}
Provides:	rhts-test-env
Obsoletes:	rhts-test-env

%description rhts
Legacy package to allow older rhts tests to run under restraint

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

# Legacy support.
ln -s rhts-environment.sh $RPM_BUILD_ROOT/usr/bin/rhts_environment.sh
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts-submit-log
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts_submit_log
ln -s rstrnt-report-result $RPM_BUILD_ROOT/usr/bin/rhts-report-result
mkdir -p $RPM_BUILD_ROOT/mnt/scratchspace
mkdir -p $RPM_BUILD_ROOT/mnt/testarea

%if 0%{?rhel}%{?fedora} > 4
# Build RHTS Selinux Testing Policy 
pushd legacy/selinux
# If dist specific selinux module is present use that.
# Why:
#  newer releases may introduce new selinux macros which are not present in
#  older releases.  This means that a module built under the newer release
#  will no longer load on an older release.  
# How:
#  Simply issue the else statement on the older release and commit the 
#  policy to git with the appropriate dist tag.
if [ -e "rhts%{?dist}.pp" ]; then
    install -p -m 644 -D rhts%{?dist}.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/rhts.pp
else
    make -f %{_datadir}/selinux/devel/Makefile
    install -p -m 644 -D rhts.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/rhts.pp
fi
popd
%endif

%post rhts
if [ "$1" -le "1" ] ; then # First install
chkconfig --level 345 restraintd on
%if 0%{?rhel}%{?fedora} > 4
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
%endif
fi

%preun rhts
if [ "$1" -lt "1" ] ; then # Final removal
chkconfig --del restraintd || :
%if 0%{?rhel}%{?fedora} > 4
semodule -r rhts || :
%endif
fi

%postun rhts
if [ "$1" -ge "1" ] ; then # Upgrade
%if 0%{?rhel}%{?fedora} > 4
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
%endif
fi

%files
%defattr(-,root,root,-)
/etc/rc.d/init.d/restraintd
%attr(0755, root, root)%{_bindir}/%{name}
%attr(0755, root, root)%{_bindir}/%{name}d
%attr(0755, root, root)%{_bindir}/rstrnt-report-result
%attr(0755, root, root)%{_bindir}/rstrnt-report-log
/usr/share/%{name}
/usr/share/%{name}/plugins/run_plugins
/usr/share/%{name}/plugins/localwatchdog
/usr/share/%{name}/plugins/report_result

%files rhts
%defattr(-,root,root,-)
%attr(0755, root, root)%{_bindir}/rhts-environment.sh
%attr(0755, root, root)%{_bindir}/rhts_environment.sh
%attr(0755, root, root)%{_bindir}/rhts-reboot
%attr(0755, root, root)%{_bindir}/rhts-report-result
%attr(0755, root, root)%{_bindir}/rhts-submit-log
%attr(0755, root, root)%{_bindir}/rhts_submit_log
%attr(0755, root, root)%{_bindir}/rhts-run-simple-test
%{_datadir}/rhts/lib/rhts-make.include
/mnt/scratchspace
%attr(1777,root,root)/mnt/testarea
%if 0%{?rhel}%{?fedora} > 4
%{_datadir}/selinux/packages/%{name}/rhts.pp
%endif

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


