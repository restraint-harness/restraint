
.PHONY: all
all:

.PHONY: install
install:
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/report_result
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/completed
	install -m0755 -d $(DESTDIR)/usr/share/restraint/plugins/localwatchdog
	install -m0755 run_plugins $(DESTDIR)/usr/share/restraint/plugins
	install -m0755 report_result/01_dmesg_check $(DESTDIR)/usr/share/restraint/plugins/report_result
	install -m0755 localwatchdog/01_sysinfo $(DESTDIR)/usr/share/restraint/plugins/localwatchdog
	install -m0755 completed/98_restore $(DESTDIR)/usr/share/restraint/plugins/completed
	install -m0755 localwatchdog/99_reboot $(DESTDIR)/usr/share/restraint/plugins/localwatchdog

.PHONY: clean
clean:
