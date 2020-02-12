Name:		restraint
Version:	0.1.45
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
# The entire source code is GPLv3+ except client/bootstrap which is MIT
License:	GPLv3+ and MIT
URL:		https://github.com/beaker-project/%{name}
Source0:	https://github.com/beaker-project/restraint/archive/%{name}-%{version}-1.tar.gz

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
Restraint harness which can run standalone or with beaker.
When provided a recipe xml it will execute each task listed in the recipe
until done.

%package client
Summary:	Used to run jobs outside of beaker
Group:		Applications/Internet
Requires:	libxslt

%description client
With the Restraint client you can run jobs outside of beaker. This will provide
the same rest API allowing all results and logs to be recorded from the test
machine.

%prep
%setup -q

%build
export CFLAGS="$RPM_OPT_FLAGS"

pushd src
make
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
%dir %{_datadir}/%{name}/
%dir %{_datadir}/%{name}/client
%dir %{_datadir}/%{name}/client/bootstrap
%dir %{_datadir}/%{name}/plugins
%dir %{_datadir}/selinux/packages/%{name}/
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
%{_datadir}/%{name}/plugins/run_plugins
%{_datadir}/%{name}/plugins/run_task_plugins
%{_datadir}/%{name}/plugins/helpers
%{_datadir}/%{name}/plugins/localwatchdog.d
%{_datadir}/%{name}/plugins/completed.d
%{_datadir}/%{name}/plugins/report_result.d
%{_datadir}/%{name}/plugins/task_run.d
%{_datadir}/%{name}/pkg_commands.d
%{_sharedstatedir}/%{name}
%{_datadir}/selinux/packages/%{name}/restraint.pp

%files client
%attr(0755, root, root)%{_bindir}/%{name}
%{_datadir}/%{name}/client/job_schema.rng
%{_datadir}/%{name}/client/bkr2rstrnt.xsl
%{_datadir}/%{name}/client/job2junit.xml
%{_datadir}/%{name}/client/job2html.xml
%{_datadir}/%{name}/client/bootstrap/LICENSE
%{_datadir}/%{name}/client/bootstrap/README
%{_datadir}/%{name}/client/bootstrap/bootstrap.min.css

%changelog
* Wed Feb 05 2020 Martin Styk <mastyk@redhat.com> 0.1.45-1
- https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-45