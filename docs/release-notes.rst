Release Notes
=============

Restraint 0.1.36
----------------

Released 24 August 2018.

* NEW: :bug:`1506064`: The dmesg error checking plugin can now match patterns
  against multi-line "cut here" style traces. The plugin now ignores a warning 
  about "mapping multiple BARs" on IBM x3250m4 systems, matching the existing 
  behaviour of the RHTS dmesg checker. (Contributed by Jacob McKenzie)
* FIXED: :bug:`1592376`: Restraint resets the SIGPIPE handler before executing
  task processes. Previously the tasks would inherit the "ignore" action for 
  SIGPIPE from the Restraint parent process, which would prevent normal shell 
  broken pipe handling from working correctly in the task. (Contributed by Matt 
  Tyson)
* FIXED: :bug:`1595167`: When the local watchdog timer expires, Restraint will
  now upload the output from :program:`journalctl` in favour of 
  :file:`/var/log/messages` if the systemd journal is present. Previously it 
  would attempt to upload :file:`/var/log/messages` even if the file did not 
  exist, causing the local watchdog handling to enter an infinite loop. 
  (Contributed by Matt Tyson)
* FIXED: :bug:`1593595`: Fixed an improper buffer allocation which could cause
  Restraint to crash with a segmentation fault instead of reporting an error 
  message in certain circumstances. (Contributed by Róman Joost)
* FIXED: :bug:`1600825`: Fixed a file conflict introduced in Restraint 0.1.35
  between the ``restraint`` package and the ``rhts-test-env`` package. 
  (Contributed by Matt Tyson)
* FIXED: :bug:`1601705`: Fixed a shell syntax error in the RPM %post scriptlet
  on RHEL4 which caused the package to be un-installable. (Contributed by Dan 
  Callaghan)
* FIXED: :bug:`1585904`: Fixed a shell syntax error in the restraintd init
  script which caused it to fail to start on RHEL4. (Contributed by Dan 
  Callaghan)

.. Not reporting bug 1603084 which was an unreleased regression

.. Not reporting bugs 1597107, 1590570 which are development improvements
   not visible to users
