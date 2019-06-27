
.PHONY: all
all:

.PHONY: install
install:
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/report_result.d
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/completed.d
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/localwatchdog.d
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 -d $(DESTDIR)/usr/share/restraint/pkg_commands.d
	install -m0755 run_plugins $(DESTDIR)/usr/share/restraint/plugins
	install -m0755 run_task_plugins $(DESTDIR)/usr/share/restraint/plugins
	install -m0755 helpers $(DESTDIR)/usr/share/restraint/plugins
	install -m0755 report_result.d/01_dmesg_check $(DESTDIR)/usr/share/restraint/plugins/report_result.d
	install -m0755 report_result.d/10_avc_check $(DESTDIR)/usr/share/restraint/plugins/report_result.d
	install -m0755 report_result.d/20_avc_clear $(DESTDIR)/usr/share/restraint/plugins/report_result.d
	install -m0755 localwatchdog.d/10_localwatchdog $(DESTDIR)/usr/share/restraint/plugins/localwatchdog.d
	install -m0755 localwatchdog.d/20_sysinfo $(DESTDIR)/usr/share/restraint/plugins/localwatchdog.d
	install -m0755 localwatchdog.d/99_reboot $(DESTDIR)/usr/share/restraint/plugins/localwatchdog.d
	install -m0755 completed.d/97_audit_rotate $(DESTDIR)/usr/share/restraint/plugins/completed.d
	install -m0755 completed.d/98_restore $(DESTDIR)/usr/share/restraint/plugins/completed.d
	install -m0755 task_run.d/05_linger $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/10_bash_login $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/15_beakerlib $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/20_unconfined $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/25_environment $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/30_restore_events $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 task_run.d/35_oom_adj $(DESTDIR)/usr/share/restraint/plugins/task_run.d
	install -m0755 pkg_commands.d/fedora $(DESTDIR)/usr/share/restraint/pkg_commands.d
	install -m0755 pkg_commands.d/rhel $(DESTDIR)/usr/share/restraint/pkg_commands.d

.PHONY: clean
clean:

.PHONY: check
check:
	make -C ../src
	cd tests && pytest-3 .
