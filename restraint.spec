%{!?_without_static:%global with_static 1}

# Got Systemd?
%if 0%{?fedora} >= 18 || 0%{?rhel} >= 7
%global with_systemd 1
%global with_selinux_policy 1
%else
%global with_systemd 0
%global with_selinux_policy 0
%endif

Name:		restraint
Version:	0.1.43
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+ and MIT
URL:		https://github.com/beaker-project/%{name}
Source0:	https://github.com/beaker-project/%{name}/%{name}-%{version}.tar.gz

%if 0%{?with_static:1}
# Sources for bundled, statically linked libraries
Source101:      libffi-3.1.tar.gz
Source102:      glib-2.56.1.tar.xz
Source103:      zlib-1.2.11.tar.gz
Source104:      bzip2-1.0.6.tar.gz
Source105:      libxml2-2.9.1.tar.gz
Source106:      curl-7.29.0.tar.bz2
Source107:      libarchive-3.2.1.tar.gz
Source108:      xz-5.2.2.tar.gz
Source109:      sqlite-autoconf-3080002.tar.gz
Source110:      intltool-0.51.0.tar.gz
Source111:      libsoup-2.48.1.tar.xz
Source112:      libssh-0.7.3.tar.xz
Source113:      cmake-3.2.3.tar.gz
Source114:      openssl-1.0.1m.tar.gz
%endif

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:	gcc
BuildRequires:	gcc-c++
BuildRequires:	pkgconfig
BuildRequires:	gettext
BuildRequires:	perl-XML-Parser
BuildRequires:	libselinux-devel
BuildRequires:	glibc-devel
BuildRequires:	make
%if 0%{?rhel}%{?fedora} > 4
BuildRequires: selinux-policy-devel
%endif
%if %{with_systemd}
BuildRequires:          systemd-units
Requires(post):         systemd
Requires(pre):          systemd
Requires(postun):       systemd
%else
Requires(post): chkconfig
Requires(preun): chkconfig
# This is for /sbin/service
Requires(preun): initscripts
Requires(postun): initscripts
%endif
%if %{with_selinux_policy}
BuildRequires: selinux-policy-devel
Requires: selinux-policy
%endif

#if not static build
%{?without_static:BuildRequires:	zlib-devel}
%{?without_static:BuildRequires:  glib2-devel}
%{?without_static:BuildRequires:  libsoup-devel}
%{?without_static:BuildRequires:  libarchive-devel}
%{?without_static:BuildRequires:  libxml2-devel}
%{?without_static:BuildRequires:  libssh-devel}
%{?without_static:BuildRequires:  openssl-devel}
%{?without_static:BuildRequires:  libcurl-devel}
BuildRequires:  make
BuildRequires:  tar

# If static build...
%if 0%{?rhel}%{?fedora} >= 6
%{?with_static:BuildRequires:	libselinux-static}
%{?with_static:BuildRequires:	glibc-static}
%endif
%{?with_static:BuildRequires:  ncurses-devel}
# intltool perl dependencies
%if 0%{?rhel}%{?fedora} >= 6
%{?with_static:BuildRequires:  perl(Getopt::Long)}
%endif
%{?with_static:BuildRequires:  perl(XML::Parser)}
%if 0%{?fedora} || 0%{?rhel} >= 8
%{?with_static:BuildRequires: python3}
%{?with_static:BuildRequires: python3-rpm-macros}
%else
%if 0%{?rhel} >= 7
%{?with_static:BuildRequires: python}
%else
# new versions of glib require python2.7 as a build dependency
%{?with_static:BuildRequires: python27}
%endif
%endif

%description
restraint harness which can run standalone or with beaker.  when provided a recipe xml it will execute
each task listed in the recipe until done.

%package rhts
Summary:	Allow unmodified rhts tests to run under restraint
Group:		Applications/Internet
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       make
%if 0%{?rhel} > 7 || 0%{?fedora} > 16
Requires:	/usr/bin/hostname
%else
Requires:       /bin/hostname
%endif
Requires:       coreutils
Requires:       libselinux-utils

# All RHTS-format task RPMs have an unversioned requirement on rhts-test-env.
# Therefore restraint-rhts provides a very low version of rhts-test-env so that
# if restraint-rhts is already installed, the dependency is satisfied without
# pulling in the real rhts-test-env, *but* if restraint-rhts is *not* already
# installed yum will prefer the real rhts-test-env. We want yum to pick the
# real rhts-test-env on traditional recipes using beah.
Provides:       rhts-test-env = 0

%description rhts
Legacy package to allow older rhts tests to run under restraint

%package client
Summary:	used to run jobs outside of beaker
Group:		Applications/Internet
Requires:	libxslt

%description client
With the restraint client you can run jobs outside of beaker.  This will provide the same
restAPI allowing all results and logs to be recorded from the test machine.

%prep
%setup -q
%if 0%{?with_static:1}
cp %{_sourcedir}/*.tar.* third-party/
%endif

%build
export CFLAGS="$RPM_OPT_FLAGS"
%if 0%{?rhel} == 5
%ifarch i386
# glib wants at least i486 for atomic instructions. RHEL6+ is already using i686.
export CFLAGS="$RPM_OPT_FLAGS -march=i486"
%endif
%endif

%if 0%{?with_static:1}
pushd third-party
%if 0%{?rhel} != 6
# If this is not RHEL6, remove the patch.
rm glib-rhel6-s390.patch
%else
%ifnarch s390x s390
# If this is RHEL6, and not a s390 machine, remove the patch.
rm glib-rhel6-s390.patch
%endif
%endif
%if 0%{?fedora} || 0%{?rhel} >= 8
make PYTHON=%{__python3}
%else
make
%endif
popd
%endif
pushd src
PKG_CONFIG_PATH=../third-party/tree/lib/pkgconfig make %{?with_static:STATIC=1}
popd
%if %{with_selinux_policy}
make -C selinux -f %{_datadir}/selinux/devel/Makefile
%endif
make -C legacy

%install
%{__rm} -rf %{buildroot}

make DESTDIR=%{buildroot} install
%if %{with_selinux_policy}
if [ -e "selinux/restraint%{?dist}.pp" ]; then
    install -p -m 644 -D selinux/restraint%{?dist}.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/restraint.pp
else
    install -p -m 644 -D selinux/restraint.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/restraint.pp
fi
%endif

make DESTDIR=%{buildroot} -C legacy install
# Legacy support.
ln -s rhts-environment.sh $RPM_BUILD_ROOT/usr/bin/rhts_environment.sh
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts-submit-log
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts_submit_log
ln -s rstrnt-backup $RPM_BUILD_ROOT/usr/bin/rhts-backup
ln -s rstrnt-restore $RPM_BUILD_ROOT/usr/bin/rhts-restore
ln -s rstrnt-reboot $RPM_BUILD_ROOT/usr/bin/rhts-reboot
ln -s rhts-sync-set $RPM_BUILD_ROOT/usr/bin/rhts-recipe-sync-set
ln -s rhts-sync-set $RPM_BUILD_ROOT/usr/bin/rhts_recipe_sync_set
ln -s rhts-sync-set $RPM_BUILD_ROOT/usr/bin/rhts_sync_set
ln -s rhts-sync-block $RPM_BUILD_ROOT/usr/bin/rhts-recipe-sync-block
ln -s rhts-sync-block $RPM_BUILD_ROOT/usr/bin/rhts_recipe_sync_block
ln -s rhts-sync-block $RPM_BUILD_ROOT/usr/bin/rhts_sync_block
ln -s rstrnt-abort $RPM_BUILD_ROOT/usr/bin/rhts-abort
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

%post
if [ "$1" -le "1" ] ; then # First install
%if %{with_systemd}
%systemd_post restraintd
# Enable restraintd by default
/bin/systemctl enable restraintd.service >/dev/null 2>&1 || :
%else
chkconfig --add restraintd
%endif
%if %{with_selinux_policy}
semodule -i %{_datadir}/selinux/packages/%{name}/restraint.pp || :
%endif
fi

%post rhts
%if 0%{?rhel}%{?fedora} > 4
if [ "$1" -le "1" ] ; then # First install
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
fi
%endif

%preun
if [ "$1" -lt "1" ] ; then # Final removal
%if %{with_systemd}
%systemd_preun %{_services}
%else
chkconfig --del restraintd || :
%endif
%if %{with_selinux_policy}
semodule -r restraint || :
%endif
fi

%preun rhts
%if 0%{?rhel}%{?fedora} > 4
if [ "$1" -lt "1" ] ; then # Final removal
semodule -r rhts || :
fi
%endif

%postun
if [ "$1" -ge "1" ] ; then # Upgrade
%if %{with_systemd}
%systemd_postun_with_restart %{_services_restart}
%else
service restraintd condrestart >/dev/null 2>&1 || :
%endif
%if %{with_selinux_policy}
semodule -i %{_datadir}/selinux/packages/%{name}/restraint.pp || :
%endif
fi

%postun rhts
%if 0%{?rhel}%{?fedora} > 4
if [ "$1" -ge "1" ] ; then # Upgrade
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
fi
%endif

%files
%defattr(-,root,root,-)
%if %{with_systemd}
%attr(0644, root, root)%{_unitdir}/%{name}d.service
%exclude %{_sysconfdir}/init.d
%else
%exclude /usr/lib/systemd
%attr(0755, root, root)%{_sysconfdir}/init.d/%{name}d
%endif
%attr(0755, root, root)%{_bindir}/%{name}d
%attr(0755, root, root)%{_bindir}/rstrnt-report-result
%attr(0755, root, root)%{_bindir}/rstrnt-report-log
%attr(0755, root, root)%{_bindir}/rstrnt-backup
%attr(0755, root, root)%{_bindir}/rstrnt-restore
%attr(0755, root, root)%{_bindir}/rstrnt-prepare-reboot
%attr(0755, root, root)%{_bindir}/rstrnt-reboot
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
%if %{with_selinux_policy}
%{_datadir}/selinux/packages/%{name}/restraint.pp
%endif

%files client
%attr(0755, root, root)%{_bindir}/%{name}
/usr/share/%{name}/client/job_schema.rng
/usr/share/%{name}/client/bkr2rstrnt.xsl
/usr/share/%{name}/client/job2junit.xml
/usr/share/%{name}/client/job2html.xml
/usr/share/%{name}/client/bootstrap/LICENSE
/usr/share/%{name}/client/bootstrap/README
/usr/share/%{name}/client/bootstrap/bootstrap.min.css

%files rhts
%defattr(-,root,root,-)
%attr(0755, root, root)%{_bindir}/rhts-environment.sh
%attr(0755, root, root)%{_bindir}/rhts-run-simple-test
%attr(0755, root, root)%{_bindir}/rhts-lint
%attr(0755, root, root)%{_bindir}/rhts-sync-set
%attr(0755, root, root)%{_bindir}/rhts-sync-block
%attr(0755, root, root)%{_bindir}/rhts-report-result
%attr(0755, root, root)%{_bindir}/rhts-flush

# Symlinks do not have attributes
%{_bindir}/rhts_sync_set
%{_bindir}/rhts_sync_block
%{_bindir}/rhts_environment.sh
%{_bindir}/rhts-reboot
%{_bindir}/rhts-submit-log
%{_bindir}/rhts_submit_log
%{_bindir}/rhts-backup
%{_bindir}/rhts-restore
%{_bindir}/rhts-recipe-sync-set
%{_bindir}/rhts_recipe_sync_set
%{_bindir}/rhts-recipe-sync-block
%{_bindir}/rhts_recipe_sync_block
%{_bindir}/rhts-abort
%{_datadir}/rhts/lib/rhts-make.include
/mnt/scratchspace
%attr(1777,root,root)/mnt/testarea
%if 0%{?rhel}%{?fedora} > 4
%{_datadir}/selinux/packages/%{name}/rhts.pp
%endif

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Dec 13 2019 Martin Styk <mastyk@redhat.com> 0.1.43-1
- Upstream release 0.1.43:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-43

* Thu Nov 07 2019 Martin Styk <mastyk@redhat.com> 0.1.42-1
- Upstream release 0.1.42:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-42

* Thu Oct 24 2019 Martin Styk <mastyk@redhat.com> 0.1.41-1
- Upstream release 0.1.41:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-41

* Wed Sep 04 2019 Martin Styk <mastyk@redhat.com> 0.1.40-1
- Upstream release 0.1.40:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-40

* Wed Feb 27 2019 Martin Styk <mastyk@redhat.com> 0.1.39-1
- Upstream release 0.1.39:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-39
* Tue Jan 29 2019 Martin Styk <mastyk@redhat.com> 0.1.38-1
- Upstream release 0.1.38:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-38

* Fri Jan 11 2019 Martin Styk <mastyk@redhat.com> 0.1.37-1
- Upstream release 0.1.37:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-37

* Fri Aug 24 2018 Dan Callaghan <dcallagh@redhat.com> 0.1.36-1
- Upstream release 0.1.36:
  https://restraint.readthedocs.io/en/latest/release-notes.html#restraint-0-1-36

* Tue Jun 12 2018 Matt Tyson <mtyson@redhat.com> 0.1.35-1
- update Github URLs (dcallagh@redhat.com)
- make rstrnt-report-result accept rhts-report-result style arguments
  (mtyson@redhat.com)
- Segfault in restraint_parse_url for host only git repositories
  (mtyson@redhat.com)
- Fix up printf arguments for report-result, make SCORE optional
  (mtyson@redhat.com)
- Fix build faults in restraint RHEL4 workarounds. (mtyson@redhat.com)
- rhts-report-result wrapper breaks on some results (mtyson@redhat.com)
- Fix glib build errors on rawhide. (mtyson@redhat.com)
- Add debug mode for makefile build (mtyson@redhat.com)
- support testinfo.desc Environment setting in Restraint. (mtyson@redhat.com)
- Restraint rpm specfile sets attributes on symlinks (mtyson@redhat.com)

* Tue Mar 20 2018 Bill Peck <bpeck@redhat.com> 0.1.34-1
- Search for first match of fragment. (bpeck@redhat.com)
- valgrind: suppress leak of static buffer in getaddrinfo() (mtyson@redhat.com)
- Update metadata parser to include RhtsRequires in dependencies.
  (bpeck@redhat.com)

* Wed Dec 13 2017 Bill Peck <bpeck@redhat.com> 0.1.33-1
- fix file:/// handling.  when host is NULL the full task path is truncated
  early. (bpeck@redhat.com)
- Support file:// path (bpeck@redhat.com)

* Tue Oct 17 2017 Bill Peck <bpeck@redhat.com> 0.1.32-1
- fix without static (bpeck@redhat.com)
- third-party: fix intltool build with Perl 5.26 (dcallagh@redhat.com)
- third-party: update to latest intltool release (dcallagh@redhat.com)

* Thu Aug 10 2017 Bill Peck <bpeck@redhat.com> 0.1.31-1
- Check for stime and etime before trying to access them. (bpeck@redhat.com)
- Fix connection retry counting (asavkov@redhat.com)
- If fragment is defined then remove fragment from path. (bpeck@redhat.com)
- Check url validity in parse_task (asavkov@redhat.com)
- Fix a segfault in openssl on ppc64. (asavkov@redhat.com)
- Bump zlib version to 1.2.11. (asavkov@redhat.com)
- fix error found by -Werror=format-security (dcallagh@redhat.com)
- third-party: remove -Wno-format in cmake (dcallagh@redhat.com)
- need python at build time, for libsoup (dcallagh@redhat.com)
- Add task start/stop times and duration to job.xml (asavkov@redhat.com)
- add softDependencies metadata (jbastian@redhat.com)

* Fri Dec 16 2016 Bill Peck <bpeck@redhat.com> 0.1.30-1
- Merge recipe roles into task roles. (asavkov@redhat.com)
- fix tasks metadata documentation to show correct example. (bpeck@redhat.com)
- Fix problem with init.d start function BZ 1351663 (jbieren@redhat.com)
- Fix http timeouts during long dependency installs. (asavkov@redhat.com)
- sync: adjust set timeouts. (asavkov@redhat.com)

* Wed Nov 23 2016 Bill Peck <bpeck@redhat.com> 0.1.29-1
- Remove sync plugin until a solution is found to mixed roles between recipes
  and guests. (bpeck@redhat.com)
- Add reconnection retries option. (asavkov@redhat.com)
- Limit number of reconnection retries. (asavkov@redhat.com)
- Fix infinite loop on ssh failure. (artem.savkov@gmail.com)
- Do not free ssh password. (asavkov@redhat.com)
- Fix segfault on failed hostname resolution. (artem.savkov@gmail.com)
- fix expected output (bpeck@redhat.com)

* Tue Nov 08 2016 Bill Peck <bpeck@redhat.com> 0.1.28-1
- rstrnt-sync block doesn't actually block. You need to keep checking on it.
  (bpeck@redhat.com)
- New linger plugin to enable session bus testing in Fedora24 and newer.
  (bpeck@redhat.com)
- update TODO list (bpeck@redhat.com)
- Docs: Add *_MEMBERS env var descriptions. (asavkov@redhat.com)
- Fix doublefree of restraint_url->uri. (asavkov@redhat.com)
- Unflat recipeSets. (asavkov@redhat.com)
- Rework roles processing. (asavkov@redhat.com)
- fix for keeping multi-host recipes in sync (bpeck@redhat.com)
- Implement timer removal. (asavkov@redhat.com)
- Capture output of make testinfo.desc (bpeck@redhat.com)
- MetaDataFetchInfo archive_entry callback. (asavkov@redhat.com)
- Switch to restraint_url instead of SoupURI. (asavkov@redhat.com)
- curl: switch to multi mode. (asavkov@redhat.com)
- Add ssl_verify parameter to fetch tag. (asavkov@redhat.com)
- Switch fetch_http to use libcurl. (asavkov@redhat.com)
- Fix rhel4 libcurl. (asavkov@redhat.com)

* Mon Oct 17 2016 Bill Peck <bpeck@redhat.com> 0.1.27-1
- Print out extracted files for dependencies as well (bpeck@redhat.com)
- Update restraint standalone to process owner attribute from job.xml  - this
  fixes a problem where $SUBMITTER is not populated when run    in stand alone
  mode. (bpeck@redhat.com)
- rstrnt-sync: close standard descriptors after fork. (asavkov@redhat.com)

* Mon Aug 08 2016 Bill Peck <bpeck@redhat.com> 0.1.26-1
- Use --whatprovides to check package installs. (asavkov@redhat.com)
- Avoid memory bug by NULL-terminating an array (pmuller@redhat.com)
- Fix suicide by ssh. (asavkov@redhat.com)
- Fix a memory leak in multipart's read_cb. (asavkov@redhat.com)

* Wed Jul 27 2016 Artem Savkov <asavkov@redhat.com> 0.1.25-2
- Fix double task_handler calls after reporting result. (asavkov@redhat.com)
- Properly process no metadata case in repodeps. (asavkov@redhat.com)

* Wed Jun 29 2016 Bill Peck <bpeck@redhat.com> 0.1.25-1
- fix restraint to report PASS or FAIL based on return code. (bpeck@redhat.com)
- Update libarchive to 3.2.1. (asavkov@redhat.com)
- Fix test_dependency. (asavkov@redhat.com)
- Add batch rpm dependency install. (asavkov@redhat.com)
- Add in audit rotate as plugin (jburke@redhat.com)
- check on correct file (bpeck@redhat.com)
- Add keepchanges documentation. (asavkov@redhat.com)
- Add libssh malformed pubkey memory corruption patch. (asavkov@redhat.com)
- build libarchive without nettle support (jbastian@redhat.com)
- Switch to own remote semaphore implementation. (asavkov@redhat.com)
- add rhts-flush as a no-op (dcallagh@redhat.com)
- use $RPM_OPT_FLAGS (dcallagh@redhat.com)
- re-enable optimizations by default (dcallagh@redhat.com)
- Update libssh to 0.7.3. (asavkov@redhat.com)
- Fix for rhts-report-result to accept the correct args (bpeck@redhat.com)
- add support for reporting tests as skipped (bpeck@redhat.com)
- Fix client return codes on failures. (asavkov@redhat.com)
- add missing underscore versions of sync/block commands (bpeck@redhat.com)
- Add recursive dependency tests. (asavkov@redhat.com)
- allow process_run to use pty or not. Default to not. Can be enabled via task
  metadata or in task param. (bpeck@redhat.com)
- Recursively check repodeps. (asavkov@redhat.com)
- Add common 'test' to test-data. (asavkov@redhat.com)
- Prepare test_dependency for recursive repodeps. (asavkov@redhat.com)
- Fix rt_sigaction valgrind suppression. (asavkov@redhat.com)
- Get rid of global session in fetch_http. (asavkov@redhat.com)
- Move metadata processing to metadata.c (asavkov@redhat.com)
- third-party: fix Makefile deps for patching targets (dcallagh@redhat.com)
- third-party: fix -Wformat-nonliteral failures with newer gcc
  (dcallagh@redhat.com)

* Thu Mar 03 2016 Bill Peck <bpeck@redhat.com> 0.1.24-1
- Relax xml schema validation to ignore unknown fields and attributes that we
  will end up ignoring anyway. (bpeck@redhat.com)
- Prepend [HOSTNAME] on to each line.  Makes multi-host recipes much easier to
  understand. (bpeck@redhat.com)
- Add beaker->restraint xslt translator. (asavkov@redhat.com)
- Don't build rhts/legacy by default. (asavkov@redhat.com)
- use https to download curl source tarball (jbastian@redhat.com)
- Don't clear "config" file on recipe complete. (bpeck@redhat.com)
- third-party: error out if tarballs fail to fetch (dcallagh@redhat.com)
- Documentation fixes. (asavkov@redhat.com)
- Add missing shebangs to bash scripts. (asavkov@redhat.com)
- Separate harness and test logs. (asavkov@redhat.com)

* Thu Jan 28 2016 Bill Peck <bpeck@redhat.com> 0.1.23-2
- third-party: xz tarball location has moved (dcallagh@redhat.com)

* Thu Jan 28 2016 Bill Peck <bpeck@redhat.com> 0.1.23-1
- Add relaxng validation. (asavkov@redhat.com)
- Fix a couple of null-pointer dereferences. (asavkov@redhat.com)
- Use precompiled selinux policy if present. (bpeck@redhat.com)
- Add retry loop for task fetches. (asavkov@redhat.com)
- Add retry loop for recipe fetches. (asavkov@redhat.com)
- Fix memleak in *_archive_read_callback. (asavkov@redhat.com)
- Fix erorneous 'failed' message during uploads. (asavkov@redhat.com)
- disabling the localwatchdog didn't actually work.  It only said it did. :-)
  (bpeck@redhat.com)
- export $TESTID even if not in rhts_compat mode (jbastian@redhat.com)
- add MIME type to links to log files (jbastian@redhat.com)
- update release version for docs (jbastian@redhat.com)
- update dependencies docs to match repoRequires wording (jbastian@redhat.com)
- document metadata repoRequires (jbastian@redhat.com)

* Tue Dec 08 2015 Bill Peck <bpeck@redhat.com> 0.1.22-1
- fix BuildRequires for older releases (bpeck@redhat.com)
- Fix upload failures with files containing special characters.
  (asavkov@redhat.com)
- Add tests for extra files deletion functionality. (asavkov@redhat.com)
- Delete extra files on fetch. (asavkov@redhat.com)
- Add unit tests for keepchanges task argument. (asavkov@redhat.com)
- Add an option to keep changes when fetching tasks. (asavkov@redhat.com)
- fetch: do not overwrite existing files. (asavkov@redhat.com)
- Fix infinite uploadloop bug. (asavkov@redhat.com)
- Support alphanumeric recipe ids. (asavkov@redhat.com)
- Implicit recipe ids support for --host option. (asavkov@redhat.com)
- Fix role variables for multihost jobs. (asavkov@redhat.com)
- libssh download location is offline (dcallagh@redhat.com)
- explicitly require perl modules for intltool (dcallagh@redhat.com)
- third-party/openssl: skip building docs (dcallagh@redhat.com)
- fix status cb to raise any WARN reported to task and recipe level.
  (bpeck@redhat.com)
- Fix memleak in build_env. (asavkov@redhat.com)
- Delete duplicate env variables. (asavkov@redhat.com)
- Fix test_env.c valgrind errors. (asavkov@redhat.com)
- Run valgrind target only on src directory. (asavkov@redhat.com)
- Add __libc_sigaction issue to valgrind supressions. (asavkov@redhat.com)
- Standalone mode authentication through libssh. (asavkov@redhat.com)
- libssh and dependencies added to third-party. (asavkov@redhat.com)
- Build target changed.  Create symlink to rhts.el6eso.pp so we install correct
  selinux policy for rhel6. (bpeck@redhat.com)
- Fix runcon test to not be noisy.  This only shows up on rhel5 or earlier.
  (bpeck@redhat.com)
- Fix jobs.rst to include information about using tarballs in fetch URL.
  (gsr@redhat.com)

* Mon Jul 27 2015 Bill Peck <bpeck@redhat.com> 0.1.21-2
- use dnf if on fedora systems (bpeck@redhat.com)
- rhel7 and fedora have hostname in /usr/bin (bpeck@redhat.com)
- ps output is too much for syslog. (bpeck@redhat.com)

* Fri Jul 17 2015 Bill Peck <bpeck@redhat.com> 0.1.21-1
- close the connection on an empty message (bpeck@redhat.com)
- fix: log separation (mkovarik@redhat.com)

* Mon Jun 29 2015 Bill Peck <bpeck@redhat.com> 0.1.20-2
- fix error passing. (bpeck@redhat.com)

* Sun Jun 28 2015 Bill Peck <bpeck@redhat.com> 0.1.20-1
- fix tight loop/memory exhaust (bpeck@redhat.com)

* Fri Jun 05 2015 Bill Peck <bpeck@redhat.com> 0.1.19-4
- add root element testsuites (mkovarik@redhat.com)
- job2junit: add support for beaker result xml (mkovarik@redhat.com)

* Thu Jun 04 2015 Bill Peck <bpeck@redhat.com> 0.1.19-3
- report localwatchdog (bpeck@redhat.com)
- fix KILLTIMEOVERRIDE (bpeck@redhat.com)

* Thu May 07 2015 Bill Peck <bpeck@redhat.com> 0.1.19-2
- RHEL4 has no libselinux-utils, only libselinux (dcallagh@redhat.com)
- Fix beaker project URL (vashirov@redhat.com)
- Add RECIPE_MEMBERS tests. (asavkov@redhat.com)
- Move build_env and related functions to separate file (asavkov@redhat.com)
- Don't write out a config file if no beaker variables are defined.
  (bpeck@redhat.com)

* Thu Apr 16 2015 Bill Peck <bpeck@redhat.com> 0.1.19-1
- Only try and adjust oom score on systems with the knob present.
  (bpeck@redhat.com)
- Check return value of fwrite to make sure we wrote everything.
  (bpeck@redhat.com)
- Make reporting cleaner byt not reporting /avc pass. (bpeck@redhat.com)
- Flush stdout and stderr before we fork.  Flush stdin after we fork.
  (bpeck@redhat.com)
- Create a new Session, this allows background processes to keep running after
  we exit. (bpeck@redhat.com)
- Fix for symlink attr warning during build (asavkov@redhat.com)
- Added server side RECIPE_MEMBERS (asavkov@redhat.com)
- fix some misc memory leaks found by valgrind (dcallagh@redhat.com)
- finish callbacks take ownership of error (dcallagh@redhat.com)
- update valgrind suppressions (dcallagh@redhat.com)
- fix test failures due to rstrnt-package (dcallagh@redhat.com)
- provide a very low version of rhts-test-env (dcallagh@redhat.com)
- Abstract install/remove package command (bpeck@redhat.com)
- Update dmesg grep to show context around matches. (bpeck@redhat.com)
- use glib format specifier macros instead of C99 ones (dcallagh@redhat.com)
- don't remove third-party dir during RPM %%install (dcallagh@redhat.com)
- RepoRequires: removing old workaround for RhtsRequires (asavkov@redhat.com)
- Conflict with rhts-test-env instead of Providing it (dcallagh@redhat.com)
- %%{sources} isn't defined on older RPM (RHEL<=5) (dcallagh@redhat.com)
- Modify oomkill behavior to prevent test harness from being oomkilled.
  (bpeck@redhat.com)
- use HTTP for tarball downloads (dcallagh@redhat.com)
- don't fetch third-party tarballs on every build (dcallagh@redhat.com)
- Oops - I ate the return code with the debug logging. (bpeck@redhat.com)
- Update remote_hup method to only check the status of tasks in its recipe
  (bpeck@redhat.com)
- Update XPath search to only tasks from the current node. (bpeck@redhat.com)
- ftp.gnome.org has moved some of the older packages to different sites.
  (bpeck@redhat.com)
- Lots of fixes for running multihost jobs under restraint controller
  (standalone mode) (bpeck@redhat.com)
- Fix spec file so that /usr/share/restraint/client doesn't get owned by both
  client and server package (bpeck@redhat.com)
- Race condition where cancellable object can get set between recipe finishing
  and client disconnecting. symptom is that next run will still have
  cancellable object set and every task will complete without running
  (bpeck@redhat.com)
- Look for RepoRequires instead of RhtsRequires. (asavkov@redhat.com)

* Wed Feb 25 2015 Bill Peck <bpeck@redhat.com> 0.1.18-1
- Clear previous beakerlib run (bpeck@redhat.com)
- previous fix showed an error where we call truncate when no file exists.
  (bpeck@redhat.com)
- Multiple fixes for errors found by covscan. (asavkov@redhat.com)
- fix warnings about unused return values from write(2) and truncate(2)
  (dcallagh@redhat.com)
- fixes for compiling --without-static, default is still to build with static
  enabled. (bpeck@redhat.com)
- Don't print non-errors to stderr since this confuses some scripts that wrap
  restraint (bpeck@redhat.com)
- Add a developer guide (asaha@redhat.com)
- Fix BuildRequires for non-static builds (asaha@redhat.com)
- Add watchdog handler to restraint client (bpeck@redhat.com)
- Implement methods for registering different callbacks based on path.
  (bpeck@redhat.com)
- tito: Do not attempt to copy existing tarballs (asaha@redhat.com)
- Minor fixes to make it work. (bpeck@redhat.com)
- Flattened commit of rstrnt-abort (bpeck@redhat.com)
- Fix process.c to close fd when the process finsihes if not already closed.
  (bpeck@redhat.com)
- Unit test to check if running a program into the background will cause us to
  hang. (bpeck@redhat.com)
- Client code doesn't use soup_server any more. (bpeck@redhat.com)
- Update to libsoup-2.48.1 (bpeck@redhat.com)
- Misc. fixes to the install doc (asaha@redhat.com)
- fix Requires for hostname (bpeck@redhat.com)
- Remove #define _BSD_SOURCE and #define _POSIX_C_SOURCE 200809L
  (bpeck@redhat.com)
- Remove main.c since it's not used any more. (bpeck@redhat.com)
- Issue #25: Add dependencies on external commands for restraint-rhts
  (asaha@redhat.com)
- Include stdint.h for uint32_t (asaha@redhat.com)
- Introduce RSTRNT_LOGGING variable to control debugging in plugins.
  (bpeck@redhat.com)
- RECIPE_MEMBERS param support. (asavkov@redhat.com)
- Fix segfault on server when client disconnects (bpeck@redhat.com)
- update restraint client. (bpeck@redhat.com)
- Fix repodeps code to not try and process none fetch method tasks.
  (bpeck@redhat.com)
- Move external watchdog from 5 minutes to 30 minutes (bpeck@redhat.com)
- 'Fragment' http tests added. (asavkov@redhat.com)
- repodeps tests in test_dependency (asavkov@redhat.com)
- http_fetch: report error on failed fragment extract (asavkov@redhat.com)
- base_path added to Recipe struct. (asavkov@redhat.com)
- Dependency test fix. (asavkov@redhat.com)
- Metadata test update (asavkov@redhat.com)
- Adjusting path_prefix_len for leading slash inconsistencies.
  (asavkov@redhat.com)
- turn off message accumulating in multipart messages (bpeck@redhat.com)
- fix boundary printing in multipart messages turn off message accumulating in
  multipart messages free client data in client disconnect method.
  (bpeck@redhat.com)
- Properly handling g_key_file* errors. (asavkov@redhat.com)
- Returning support for nonfragment http urls. (asavkov@redhat.com)
- repoRequires http[s] support (asavkov@redhat.com)
- Adding support for repoDeps metadata option. (asavkov@redhat.com)
- server post multipart fixes (asavkov@redhat.com)
- client post multipart fixes (asavkov@redhat.com)
- Clear error in recipe_handler_finish (bpeck@redhat.com)
- Update restraint client to use new streaming support (bpeck@redhat.com)
- Support for streaming data back to client (bpeck@redhat.com)
- Change the heartbeat from 5 minutes to 1 minute (bpeck@redhat.com)
- Implement restraint_append_message. (bpeck@redhat.com)
- Don't call restraint_queue_message directly. (bpeck@redhat.com)
- fix copy_header to update Location base (bpeck@redhat.com)
- Allow a recipe to be cancelled. (bpeck@redhat.com)
- set to task to NULL. (bpeck@redhat.com)
- Update ignore list to latest third-party packages (bpeck@redhat.com)
- new restraint_config_trunc to clear the configuration (bpeck@redhat.com)
- Bump to newer version of libsoup which includes some important fixes.
  (bpeck@redhat.com)
- Include the run levels and add the service instead (bpeck@redhat.com)
- make selinux policy optional. (bpeck@redhat.com)
- restraintd: https support (asavkov@redhat.com)
- restraintd: fetch_http premature error cleanup fix (asavkov@redhat.com)
- Fix for issue#15. Client retcode on failed get_addr. (asavkov@redhat.com)

* Wed Sep 17 2014 Bill Peck <bpeck@redhat.com> 0.1.17-3
- Fixed client segfault on xml without recipe ids. (asavkov@redhat.com)
- Fix typo in restraint word (gsr@redhat.com)
- Handle no dependencies in metadata correctly (asaha@redhat.com)
- Minor fix in the install from sources doc (asaha@redhat.com)
- Fix typo in service start command. (gsr@redhat.com)
- Restraint command to convert the stand alone job.xml to junit results.
  (gsr@redhat.com)

* Fri Sep 12 2014 Bill Peck <bpeck@redhat.com> 0.1.17-2
- Update Authors (bpeck@redhat.com)
- Recipe roles in client. (asavkov@redhat.com)
- Fix duplicate task/params in client. (asavkov@redhat.com)
- fix restraint client help (bpeck@redhat.com)

* Wed Sep 10 2014 Bill Peck <bpeck@redhat.com> 0.1.17-1
- Include an xslt template which will convert job.xml to junit.xml
  (bpeck@redhat.com)
- Bug: 1077115, fix blatently stole from beah fix (bpeck@redhat.com)
- If staf is installed then start it (bpeck@redhat.com)
- RecipeData init while copying xml template. (asavkov@redhat.com)
- Untie recipe_id from rundir_id. (asavkov@redhat.com)
- Documentation update. (asavkov@redhat.com)
- Creating task/params in copy_job_as_template(). (asavkov@redhat.com)
- Quitting loop on last aborted task. (asavkov@redhat.com)
- Switched to ids instead of wboards (asavkov@redhat.com)
- copying "role" attr in copy_task_nodes() (asavkov@redhat.com)
- "port" option fixed (asavkov@redhat.com)
- restraint client use a single "server" (asavkov@redhat.com)
- Log paths in job.xml fix (asavkov@redhat.com)
- Fixed premature quit on a problem with a single recipe. (asavkov@redhat.com)
- exit if failed to init recipe (asavkov@redhat.com)
- Cleaning up extra whiteboards. (asavkov@redhat.com)
- Default wboard value for recipes with undefined wb. (asavkov@redhat.com)
- parse_new_job() wboard memleak fix (asavkov@redhat.com)
- Initial mh support for restraint client. (asavkov@redhat.com)
- restraintd: config_port memleak fix (asavkov@redhat.com)
- Make find_recipe() return specific recipe. (asavkov@redhat.com)
- Storing actual uri in recipe_hosts hash table. (asavkov@redhat.com)
- Added host cmd line option to restraint client. (asavkov@redhat.com)
- Merge pull request #12 from jstancek/ppc64le_v1 (bill@pecknet.com)
- cmd_result tried to free a pointer returned by getenv in case when outputfile
  is supplied by env variable. filename and outputfilename vars are now freed
  in callback_outputfile prior to another allocation. (bpeck@redhat.com)
- Fix segfault if task->metadata is NULL also fix parse_time_string to allow
  for raw seconds to be passed in. (bpeck@redhat.com)
- fix test_fetch_git to propagate error (bpeck@redhat.com)
- Bunch of cmd_result fixes. (asavkov@redhat.com)
- Bunch of cmd_log fixes. (asavkov@redhat.com)
- Bunch of cmd_watchdog fixes. (asavkov@redhat.com)
- Proper cleanup of parsed arguments. (asavkov@redhat.com)
- Proper freeing of restraint client's AppData (asavkov@redhat.com)
- Fixed xmlXPathObjectPtr leak in parse_new_job() (asavkov@redhat.com)
- client.c multiple ghashtable memleaks fixed. (asavkov@redhat.com)
- client.c multiple gobject memleaks fixed. (asavkov@redhat.com)
- client.c multiple string memleaks fixed. (asavkov@redhat.com)
- Move unref to after if statement since on error reqh will be none
  (bpeck@redhat.com)
- Update skipped tests when thttpd is not installed (bpeck@redhat.com)
- Fix memory leaks in test_dependency test case (bpeck@redhat.com)
- Merge pull request #8 from sm00th/memleaks (bill@pecknet.com)
- Fixed premature g_clear_error() (asavkov@redhat.com)
- Fixed infinite loop in task_handler() (bpeck@redhat.com)
- Freeing task metadata on task free. (asavkov@redhat.com)
- Added libsoup valgrind suppressions. (asavkov@redhat.com)
- Freeing task_run_data in task_finish_plugin_callback() (asavkov@redhat.com)
- Freeing server_data in plugin_finish_callback() (asavkov@redhat.com)
- Fixed string leak in server_recipe_callback() (asavkov@redhat.com)
- Fixed iochannel leak in process_run() (asavkov@redhat.com)
- Fixed string leak in message_complete() (asavkov@redhat.com)
- config.c error leak fix (asavkov@redhat.com)
- fixed GHashTable leak in server_control_callback() (asavkov@redhat.com)
- freeing temp string in restraint_task_watchdog() (asavkov@redhat.com)
- recipe_handler() proper cleanup in RECIPE_RUNNING state (asavkov@redhat.com)
- restraintd server graceful exit on termination. (asavkov@redhat.com)
- Rework install_dependencies to make it so we can unit test it.
  (bpeck@redhat.com)
- restructure of metadata values (bpeck@redhat.com)
- xmlparser free order fix in recipe_handler() (asavkov@redhat.com)
- g_string memleak fix in recipe_handler and task_handler (asavkov@redhat.com)
- add support for ppc64le (jstancek@redhat.com)
- test_packages is covered under test_process (bpeck@redhat.com)
- refactored async calls to be more like async calls. :-) (bpeck@redhat.com)
- rework fetch_git code to be independent and use callbacks for logic.
  (bpeck@redhat.com)
- Update Authors file (bpeck@redhat.com)
- Update TODO (bpeck@redhat.com)
- fix memory leaks (bpeck@redhat.com)
- fix uploading of result logs (bpeck@redhat.com)
- minor cleanup.  we don't have to worry about initializing localwatchdog since
  it's set in process_finish_callback and if we fail to fork it will get
  asseted on success. (bpeck@redhat.com)
- fix check target in Makefiles (bpeck@redhat.com)
- Added local watchdog pass/fail to test_process.c
  (jbieren@dhcp47-171.desklab.eng.bos.redhat.com)
- Added test_process (bpeck@redhat.com)
- Allow beaker lab controller and recipe variables to be overridden via cmdline
  (bpeck@redhat.com)
- Change rstrnt-report-result to use filename when -o is used.
  (bpeck@redhat.com)
- Updates using restraint docs. (bpeck@redhat.com)

* Tue Jun 03 2014 Bill Peck <bpeck@redhat.com> 0.1.16-1
- client package doesn't need the daemon to be installed. (bpeck@redhat.com)
- fix possible memory leak when trying to open both ipv4 and ipv6 sockets
  (bpeck@redhat.com)
- use correct path to job.xml and for index.html (jbastian@redhat.com)
- use index.html as output instead of results.html (jbastian@redhat.com)
- update Makefiles to install client files (jbastian@redhat.com)
- updates for move to /usr/share/restraint/client (jbastian@redhat.com)
- move client files out of docs (jbastian@redhat.com)
- generate pretty html from job.xml (jbastian@redhat.com)
- update version string in path to CSS files (jbastian@redhat.com)
- update rpm spec for bootstrap CSS files (jbastian@redhat.com)
- add local copy of bootstrap min CSS source (jbastian@redhat.com)
- add link to bootstrap CSS framework (jbastian@redhat.com)

* Thu May 29 2014 Jeff Bastian <jbastian@redhat.com> 0.1.15-2
- add local copy of boostrap CSS
- automatically generate results.html from job.xml (when running from client)

* Wed May 28 2014 Bill Peck <bpeck@redhat.com> 0.1.15-1
- Add IPv6 support (bpeck@redhat.com)
- If we fail to send a message because of a client issue, don't keep retrying.
  (bpeck@redhat.com)
- Remove hard coded server port. (bpeck@redhat.com)

* Mon May 05 2014 Bill Peck <bpeck@redhat.com> 0.1.14-1
- Updated usecases (bpeck@redhat.com)
- Allow multiple versions of restraint to run if different ports are used.
  (bpeck@redhat.com)
- install task run plugin (bpeck@redhat.com)
- update TODO file (bpeck@redhat.com)
- Update features list (bpeck@redhat.com)
- Updates to plugin docs to match newest plugins. (bpeck@redhat.com)
- Add git clone info to docs (bpeck@redhat.com)
- Setup some environment variables if they aren't defined. Make the plugin
  reporting a little clearer. (bpeck@redhat.com)
- fixes for watchdog adjust (bpeck@redhat.com)
- final pieces to support extending watchdog from reserve task.
  (bpeck@redhat.com)
- Allow tasks to disable localwatchdog. (bpeck@redhat.com)
- Only report denials in avc_check. (bpeck@redhat.com)
- Update static build requires to pick up fedora (bpeck@redhat.com)
- Pass OUTPUTFILE to rstrnt-report-result in case the variable wasn't exported
  (bpeck@redhat.com)
- minor doc updates (bpeck@redhat.com)
- Provide SUBMITTER for legacy rhts and RSTRNT_OWNER for new.
  (bpeck@redhat.com)
- minor fix for RSTRNT_RECIPE_URL (bpeck@redhat.com)
- Another variable that legacy rhts expects. (bpeck@redhat.com)
- define RSTRNT_RECIPE_URL so that tasks can request the current recipe xml.
  (bpeck@redhat.com)
- Define RESULT_SERVER for rhts legacy tasks.  The value shouldn't matter.
  (bpeck@redhat.com)
- Display the commands being executed (bpeck@redhat.com)
- License Restraint under the GPL 3.0 (bpeck@redhat.com)
- Propagate the error when we fail a command returns non-zero
  (bpeck@redhat.com)

* Thu Apr 17 2014 Bill Peck <bpeck@redhat.com> 0.1.13-1
- cosmetic change (bpeck@redhat.com)
- Fix guint64 mistake (bpeck@redhat.com)
- report the number seconds we are pushing the watchdog out.  Should help with
  debugging. (bpeck@redhat.com)
- Support added to allow tasks to use an alternate max time via task params.
  looks for both KILLTIMEOVERRIDE and RSTRNT_MAX_TIME (bpeck@redhat.com)
- fixes to %%post scripts (bpeck@redhat.com)
- Minor updates to doc. (bpeck@redhat.com)
- Always populate the RSTRNT_* variables to make it easier to migrate.
  (bpeck@redhat.com)
- Fix %%post scripts to start restraintd (bpeck@redhat.com)
- Updates to the documentation (bpeck@redhat.com)
- Add beakerlib reporting support (bpeck@redhat.com)
- Fix env variable generation when prefix is NULL. (bpeck@redhat.com)
- Minor update to expected output (bpeck@redhat.com)

* Wed Apr 09 2014 Bill Peck <bpeck@redhat.com> 0.1.12-1
- error checking on client to make sure we can make the run dir.
  (bpeck@redhat.com)
- Better client feedback (bpeck@redhat.com)
- Don't create variables if value is NULL fix metadata parser to use defaults
  if key is missing. (bpeck@redhat.com)

* Thu Apr 03 2014 Bill Peck <bpeck@redhat.com> 0.1.11-1
- only pass disable_plugin if plugins have been disabled (bpeck@redhat.com)
- add dummy rhts-lint to allow us to make testinfo.desc (bpeck@redhat.com)
- Updated service file to depend on network being online (bpeck@redhat.com)
- separate restraint into client package (bpeck@redhat.com)
- Report any errors at recipe level. (bpeck@redhat.com)

* Mon Mar 31 2014 Bill Peck <bpeck@redhat.com> 0.1.10-1
- allow report_plugins to be disabled (bpeck@redhat.com)
- fix TESTID setting (bpeck@redhat.com)
- Add TESTID for legacy/beakerlib runs. (bpeck@redhat.com)
- Added selinux plugin check (bpeck@redhat.com)

* Tue Mar 25 2014 Bill Peck <bpeck@redhat.com> 0.1.9-1
- Update the documentation to match how restraint works stand alone now.
  (bpeck@redhat.com)
- minor output change (bpeck@redhat.com)
- minor tweaks to plugins to use proper names (bpeck@redhat.com)
- update gitignore and remove docs/_build (bpeck@redhat.com)
- fix logging (bpeck@redhat.com)
- Install task dependencies, this is important for running outside of beaker.
  (bpeck@redhat.com)
- minor grammer and spelling mistakes (bpeck@redhat.com)
- Started docs.  Documented plugins. (bpeck@redhat.com)

* Thu Mar 20 2014 Bill Peck <bpeck@redhat.com> 0.1.8-1
- allow client.c to compile on rhel4 and exclude /usr/lib/systemd on none-
  systemd systems (bpeck@redhat.com)

* Thu Mar 20 2014 Bill Peck <bpeck@redhat.com> 0.1.7-1
- fixes for systemd (bpeck@redhat.com)
- use /var/lib/restraint for running state config. also record logs in
  standalone job xml. (bpeck@redhat.com)
- fix comment (bpeck@redhat.com)
- Log errors to /dev/console and all messages to /var/log/restraintd.log
  (bpeck@redhat.com)
- Changes to support running stand alone better (bpeck@redhat.com)
- disable g_io_channel decoding which can barf on binary data.
  (bpeck@redhat.com)

* Thu Jan 23 2014 Bill Peck <bpeck@redhat.com> 0.1.6-1
- revert spec file changes so tito doesn't get confused. (bpeck@redhat.com)
- fix legacy report_result.  Was not passing args with quotes
  (bpeck@redhat.com)
- patch to fix /usr/lib64 to /usr/lib for libffi (jbastian@redhat.com)
- build libxml2 without python support (jbastian@redhat.com)
- also update libxml2-2.9.1 for aarch64 (jbastian@redhat.com)
- update third-party to libffi-3.0.13 (jbastian@redhat.com)
- Add IPV6 to TODO list (bpeck@redhat.com)
- Add TODO list. (bpeck@redhat.com)

* Tue Jan 21 2014 Bill Peck <bpeck@redhat.com> 0.1.5-1
- fix make clean targets (bpeck@redhat.com)
- fix fd leak (bpeck@redhat.com)
- fix path for backup/restore (bpeck@redhat.com)
- More legacy variables (bpeck@redhat.com)
- fix reporting of which plugins are in use. (bpeck@redhat.com)
- Fix entry_point to be an array of strings to pass into process
  (bpeck@redhat.com)
- Introduced task runner plugins (bpeck@redhat.com)
- Fix running plugins (both localwatchdog and report_result) (bpeck@redhat.com)
- use NULL term value (bpeck@redhat.com)

* Thu Jan 09 2014 Bill Peck <bpeck@redhat.com> 0.1.4-1
- Move localwatchdog plugins to a more generic plugins model.
  (bpeck@redhat.com)
- Provide TESTNAME and TESTPATH for legacy rhts (bpeck@redhat.com)
- Update build instructions (bpeck@redhat.com)
- ignore whitespace in scan. (bpeck@redhat.com)
- Look for a guestrecipe if we fail to find a regular recipe.
  (bpeck@redhat.com)
- Include rhts-run-simple-test from rhts. (bpeck@redhat.com)
- Update ignore to restrnt names (bpeck@redhat.com)
- Remember localwatchdog state, the plugins may reboot the system.
  (bpeck@redhat.com)
- disable requeueing messages for now. (bpeck@redhat.com)
- use guint64 (bpeck@redhat.com)
- Move rhts legacy into a sub-package (bpeck@redhat.com)
- Activate service after install (bpeck@redhat.com)
- fix buildroot (bpeck@redhat.com)
- Install localwatchdog plugins (bpeck@redhat.com)

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


