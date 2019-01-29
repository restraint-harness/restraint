Release Notes
=============

Restraint 0.1.38
----------------

Released 29 January 2019.

* | FIXED: :bug:`1670111`: Fixed crash of Restraint for ppc64le and aarch64
    architecture.
  | (Contributed by Bill Peck)

Restraint 0.1.37
----------------

Released 11 January 2019.

* | NEW: :bug:`1665390`: Added feature to set family from client XML
  | (Contributed by Bill Peck)
* | NEW: :bug:`1656466`: Restraint now supports ``@module`` syntax for
    dependencies for RHEL8+.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1663125`: Restraint now listens separately for IPv4 and IPv6. One
    running version of the protocol is sufficient for ``restraintd`` run.
  | (Contributed by Bill Peck)
* | FIXED: :bug:`1663825`: When BootCurrent is not available, Restraint will
    try to fall back to :file:`/root/EFI_BOOT_ENTRY.TXT`.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1659353`: Fixed obsolete URL for Bzip2 package in Makefile.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1599550`: Fixed crash of Restraint for RHEL6 arch s390 caused
    by glib2.
  | (Contributed by Matt Tyson)
* | FIXED: :bug:`1608262`: Fixed guest-host synchronization.
  | (Contributed by Dan Callaghan)


Restraint 0.1.36
----------------

Released 24 August 2018.

* | NEW: :bug:`1506064`: The dmesg error checking plugin can now match patterns
    against multi-line "cut here" style traces. The plugin now ignores a warning
    about "mapping multiple BARs" on IBM x3250m4 systems, matching the existing
    behaviour of the RHTS dmesg checker.
  | (Contributed by Jacob McKenzie)

* | FIXED: :bug:`1592376`: Restraint resets the SIGPIPE handler before executing
    task processes. Previously the tasks would inherit the "ignore" action for
    SIGPIPE from the Restraint parent process, which would prevent normal shell
    broken pipe handling from working correctly in the task.
  | (Contributed by Matt Tyson)
* | FIXED: :bug:`1595167`: When the local watchdog timer expires, Restraint will
    now upload the output from :program:`journalctl` in favour of
    :file:`/var/log/messages` if the systemd journal is present. Previously it
    would attempt to upload :file:`/var/log/messages` even if the file did not
    exist, causing the local watchdog handling to enter an infinite loop.
  | (Contributed by Matt Tyson)
* | FIXED: :bug:`1593595`: Fixed an improper buffer allocation which could cause
    Restraint to crash with a segmentation fault instead of reporting an error
    message in certain circumstances.
  | (Contributed by Róman Joost)
* | FIXED: :bug:`1600825`: Fixed a file conflict introduced in Restraint 0.1.35
    between the ``restraint`` package and the ``rhts-test-env`` package.
  | (Contributed by Matt Tyson)
* | FIXED: :bug:`1601705`: Fixed a shell syntax error in the RPM %post scriptlet
    on RHEL4 which caused the package to be un-installable.
  | (Contributed by Dan Callaghan)
* | FIXED: :bug:`1585904`: Fixed a shell syntax error in the restraintd init
    script which caused it to fail to start on RHEL4.
  | (Contributed by Dan Callaghan)

.. Not reporting bug 1603084 which was an unreleased regression

.. Not reporting bugs 1597107, 1590570 which are development improvements
   not visible to users
