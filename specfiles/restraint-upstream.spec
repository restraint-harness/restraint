Name:		restraint
Version:	0.4.13
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+ and MIT
URL:		https://github.com/beaker-project/%{name}
Source0:	https://github.com/beaker-project/restraint/archive/%{name}-%{version}.tar.gz

BuildRequires:		gcc
BuildRequires:		gcc-c++
BuildRequires:		pkgconfig
BuildRequires:		gettext
BuildRequires:		perl-XML-Parser
BuildRequires:		libselinux-devel
BuildRequires:		glibc-devel
BuildRequires:		make
BuildRequires:		autoconf
BuildRequires:		selinux-policy-devel
BuildRequires:		systemd-units
BuildRequires:		zlib-devel
BuildRequires:		glib2-devel
BuildRequires:		libsoup-devel
BuildRequires:		libarchive-devel
BuildRequires:		libxml2-devel
BuildRequires:		json-c-devel
BuildRequires:		openssl-devel
BuildRequires:		libcurl-devel
BuildRequires:		make
BuildRequires:		tar
BuildRequires:		selinux-policy-devel

Requires(post):		systemd
Requires(pre):		systemd
Requires(postun):	systemd
Requires:		selinux-policy

%description
restraint harness which can run standalone or with beaker.  when provided a recipe xml it will execute
each task listed in the recipe until done.

%package client
Summary:	used to run jobs outside of beaker
Group:		Applications/Internet
Requires:	libxslt

%description client
With the restraint client you can run jobs outside of beaker.  This will provide the same
restAPI allowing all results and logs to be recorded from the test machine.

%prep
%setup -q

%build
export CFLAGS="$RPM_OPT_FLAGS"

pushd src
make %{?_smp_mflags}
popd

make -C selinux -f %{_datadir}/selinux/devel/Makefile

%install
make DESTDIR=$RPM_BUILD_ROOT install
if [ -e "selinux/restraint%{?dist}.pp" ]; then
    install -p -m 644 -D selinux/restraint%{?dist}.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/restraint.pp
else
    install -p -m 644 -D selinux/restraint.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/restraint.pp
fi

%post
if [ "$1" -le "1" ] ; then # First install
%systemd_post restraintd
# Enable restraintd by default
/bin/systemctl enable restraintd.service >/dev/null 2>&1 || :
semodule -i %{_datadir}/selinux/packages/%{name}/restraint.pp || :
fi

%preun
if [ "$1" -lt "1" ] ; then # Final removal
%systemd_preun %{_services}
semodule -r restraint || :
fi

%postun
if [ "$1" -ge "1" ] ; then # Upgrade
%systemd_postun_with_restart %{_services_restart}
semodule -i %{_datadir}/selinux/packages/%{name}/restraint.pp || :
fi

%files
%defattr(-,root,root,-)
%attr(0644, root, root)%{_unitdir}/%{name}d.service
%exclude %{_sysconfdir}/init.d
%attr(0755, root, root)%{_bindir}/%{name}d
%attr(0755, root, root)%{_bindir}/rstrnt-report-result
%attr(0755, root, root)%{_bindir}/rstrnt-report-log
%attr(0755, root, root)%{_bindir}/rstrnt-backup
%attr(0755, root, root)%{_bindir}/rstrnt-restore
%attr(0755, root, root)%{_bindir}/rstrnt-prepare-reboot
%attr(0755, root, root)%{_bindir}/rstrnt-reboot
%attr(0755, root, root)%{_bindir}/rstrnt-sync-set
%attr(0755, root, root)%{_bindir}/rstrnt-sync-block
%attr(0755, root, root)%{_bindir}/check_beaker
%attr(0755, root, root)%{_bindir}/rstrnt-adjust-watchdog
%attr(0755, root, root)%{_bindir}/rstrnt-abort
%attr(0755, root, root)%{_bindir}/rstrnt-sync
%attr(0755, root, root)%{_bindir}/rstrnt-package
/usr/share/%{name}/plugins/run_plugins
/usr/share/%{name}/plugins/run_task_plugins
/usr/share/%{name}/plugins/helpers
/usr/share/%{name}/plugins/localwatchdog.d
/usr/share/%{name}/plugins/completed.d
/usr/share/%{name}/plugins/report_result.d
/usr/share/%{name}/plugins/task_run.d
/usr/share/%{name}/pkg_commands.d
/var/lib/%{name}
%{_sysconfdir}/%{name}
%{_datadir}/selinux/packages/%{name}/restraint.pp

%files client
%attr(0755, root, root)%{_bindir}/%{name}
/usr/share/%{name}/client/job_schema.rng
/usr/share/%{name}/client/bkr2rstrnt.xsl
/usr/share/%{name}/client/job2junit.xml
/usr/share/%{name}/client/job2html.xml
/usr/share/%{name}/client/bootstrap/LICENSE
/usr/share/%{name}/client/bootstrap/README
/usr/share/%{name}/client/bootstrap/bootstrap.min.css

%changelog
