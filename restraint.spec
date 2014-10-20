
# Got Systemd?
%if 0%{?fedora} >= 18 || 0%{?rhel} >= 7
%global with_systemd 1
%global with_selinux_policy 1
%else
%global with_systemd 0
%global with_selinux_policy 0
%endif

Name:		restraint
Version:	0.1.17
Release:	3%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+ and MIT
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
Requires: selinux-policy >= %{_selinux_policy_version}
%endif

#if not static build
BuildRequires:	zlib-devel
# If static build...
%if 0%{?rhel}%{?fedora} >= 6
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
%if %{with_selinux_policy}
make -C selinux -f %{_datadir}/selinux/devel/Makefile
%endif

%install
%{__rm} -rf %{buildroot}

pushd third-party
make clean
popd
make DESTDIR=%{buildroot} install
%if %{with_selinux_policy}
install -p -m 644 -D selinux/restraint.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/restraint.pp
%endif

# Legacy support.
ln -s rhts-environment.sh $RPM_BUILD_ROOT/usr/bin/rhts_environment.sh
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts-submit-log
ln -s rstrnt-report-log $RPM_BUILD_ROOT/usr/bin/rhts_submit_log
ln -s rstrnt-report-result $RPM_BUILD_ROOT/usr/bin/rhts-report-result
ln -s rstrnt-backup $RPM_BUILD_ROOT/usr/bin/rhts-backup
ln -s rstrnt-restore $RPM_BUILD_ROOT/usr/bin/rhts-restore
ln -s rstrnt-reboot $RPM_BUILD_ROOT/usr/bin/rhts-reboot
ln -s rhts-sync-set $RPM_BUILD_ROOT/usr/bin/rhts-recipe-sync-set
ln -s rhts-sync-block $RPM_BUILD_ROOT/usr/bin/rhts-recipe-sync-block
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
if [ "$1" -le "1" ] ; then # First install
%if 0%{?rhel}%{?fedora} > 4
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
%endif
fi

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
if [ "$1" -lt "1" ] ; then # Final removal
%if 0%{?rhel}%{?fedora} > 4
semodule -r rhts || :
%endif
fi

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
if [ "$1" -ge "1" ] ; then # Upgrade
%if 0%{?rhel}%{?fedora} > 4
semodule -i %{_datadir}/selinux/packages/%{name}/rhts.pp || :
%endif
fi

%files
%defattr(-,root,root,-)
%if %{with_systemd}
%attr(0755, root, root)%{_unitdir}/%{name}d.service
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
%attr(0755, root, root)%{_bindir}/rstrnt-reboot
%attr(0755, root, root)%{_bindir}/check_beaker
%attr(0755, root, root)%{_bindir}/rstrnt-adjust-watchdog
/usr/share/%{name}
/usr/share/%{name}/plugins/run_plugins
/usr/share/%{name}/plugins/run_task_plugins
/usr/share/%{name}/plugins/localwatchdog.d
/usr/share/%{name}/plugins/completed.d
/usr/share/%{name}/plugins/report_result.d
/usr/share/%{name}/plugins/task_run.d
/var/lib/%{name}
%if %{with_selinux_policy}
%{_datadir}/selinux/packages/%{name}/restraint.pp
%endif

%files client
%attr(0755, root, root)%{_bindir}/%{name}
/usr/share/%{name}/client/job2junit.xml
/usr/share/%{name}/client/job2html.xml
/usr/share/%{name}/client/bootstrap/LICENSE
/usr/share/%{name}/client/bootstrap/README
/usr/share/%{name}/client/bootstrap/bootstrap.min.css

%files rhts
%defattr(-,root,root,-)
%attr(0755, root, root)%{_bindir}/rhts-environment.sh
%attr(0755, root, root)%{_bindir}/rhts_environment.sh
%attr(0755, root, root)%{_bindir}/rhts-reboot
%attr(0755, root, root)%{_bindir}/rhts-report-result
%attr(0755, root, root)%{_bindir}/rhts-submit-log
%attr(0755, root, root)%{_bindir}/rhts_submit_log
%attr(0755, root, root)%{_bindir}/rhts-run-simple-test
%attr(0755, root, root)%{_bindir}/rhts-backup
%attr(0755, root, root)%{_bindir}/rhts-restore
%attr(0755, root, root)%{_bindir}/rhts-restore
%attr(0755, root, root)%{_bindir}/rhts-lint
%attr(0755, root, root)%{_bindir}/rhts-sync-set
%attr(0755, root, root)%{_bindir}/rhts-sync-block
%attr(0755, root, root)%{_bindir}/rhts-recipe-sync-set
%attr(0755, root, root)%{_bindir}/rhts-recipe-sync-block
%{_datadir}/rhts/lib/rhts-make.include
/mnt/scratchspace
%attr(1777,root,root)/mnt/testarea
%if 0%{?rhel}%{?fedora} > 4
%{_datadir}/selinux/packages/%{name}/rhts.pp
%endif

%clean
%{__rm} -rf %{buildroot}

%changelog
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


