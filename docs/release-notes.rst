Release Notes
=============

Restraint 0.1.43
----------------
* | FIXED: :bug:`1774211`: Seeing too many repo extraction
    Under certain conditions, restraint was failing to go
    to next repoRequires operation causing redundant
    fetch operations to occur.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1236568`: Separate dmesg clear from check
    Need for a separate plugin so clear of the dmesg logs
    is done independently from check dmesg logs.
    Currently this is done during `dmesg check` plugin.
    If `dmesg check` plugin is disabled, so is the clear
    operation leaving the next task will process unrelated
    errors. By separating clear from check operation, the clear
    operation can always be performed.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1749316`: Rstrnt retry refresh role on socket io err
    User periodically observed "Error: Socket I/O Timed out".
    This occurred during the restraint task state
    "** Refreshing peer role hostnames" which collects
    host roles from lab controller and there is no response
    in default 1 minute time frame.  To handle network
    issues, restraint will retry this event similar to
    what is done when performing fetch operations.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1762731`: Rstrnt add more metadata UTs
  | (Contributed by Carol Bouchard)
* | NEW: :bug:`1455763`: New command rstrnt-prepare-reboot
    It does the same preparatory work as rstrnt-reboot, but does not
    trigger the reboot. Tasks can use this prior to (intentionally)
    crashing the system or rebooting it in some other non-standard
    way.
  | (Contributed by Tomas Klohna)

Restraint 0.1.42
----------------
* | FIXED: :bug:`1753652`: Multihost Sync Improvements
    A number of improvements have been made to the Multihost
    synchronization feature.
    * Only perform multihost sync when roles SERVERS and CLIENTS
      are defined in the environment.
    * Add the ability to tune the amount of time to pause before
      another retry attempt.
    * Restraint's retry pause time reduced to 30 from 60.
    * Improve log entries to provide insight to multihost sync
      operations.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1756515`: FALSESTRINGS not provide consistent results
    If a dmesg log contains  "falsestring failurestring", then
    falsestring will override failurestring.  If they were
    swapped where "failurestring falsestring", then falsestring
    does not override failurestring which is a bug.  This
    changeset resolves this inconsistency.  It also removed
    printing of surrounding 5 lines around the matching line.
    This will make it easier for users to identify which line
    has matched.  The full dmesg log file is also provided so
    user can easily search through the full dmesg log if they
    need to see surrounding lines.
  | (Contributed by Carol Bouchard)

Restraint 0.1.41
----------------
* | FIXED: :bug:`1753336`: The cli rstrnt-adjust-watchdog command
    was producing random results.  The message from restraintd
    to the lab controller was getting truncated when the number
    of digits for time increased.  There is an extra 30 minutes
    added to this message for external watchdog so it is possible
    for it to increase by 1 byte. Since restraintd used the same
    message received for the request, the message length was
    already set so the soup library didn't try to recalculate it.
    The solution is to initialize the length to 0 to force the
    soup library to recalculate it.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1751074`: Rlse 0.1.40 seeing a lot of invalid
    dmesg failures.  This behavior only occurs on x86_64 arch.
    The rpm task /distribution/install, method VirtWorkaround()
    is setting an empty /usr/share/rhts/failurestrings file.
    As a result, every line is treated as a failure. Solution
    is to make sure the failurestrings file has content
    before using it.
    Included in this changeset is detail output for next triage.
    This output is written to the bottom of resultoutputfile.log when
    01_dmesg_check reports failure.  This debug code reports which
    set of failure and falsestring data was used: environment vars,
    files, or hardcoded defaults.  It shows content of the
    failure/falsestrings variables and if the files exist, if there
    is data in them or the files content is also dumped into the
    bottom of the log file.
  | (Contributed by Carol Bouchard)

Restraint 0.1.40
----------------

Released 4 September 2019.

* | FIXED: :bug:`1609330`: Restraint should have a log similar to
    beah's /mnt/testarea/current.log.  This file points to unique
    task file named /tmp/tmp.XXXX (where XXXX is random).  As tasks
    change, the link changes to new tmp.XXXX file.  File
    current.log makes it convenient to find current task log file
    as the job is running.
  | (Contributed by Carol Bouchard)
* | NEW: :bug:`1713313`: Provide an option for not rebooting the
    test box after localwatchdog killed a task. No new code was
    written for this since an option already existed.  This
    changeset documents the option `RSTRNT_DISABLED` which allows
    the user to disable specified plugins.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1678549`: Restraint starts too early for the system
    to get ready for testing.  Instead, wait until network is up
    before starting restraint.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1694221`: SELinux tests break. The `20_unconfined` plugin
    currently checks if process running with SELinux role and domain but
    was missing check if user is SELinux user.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1478653`: [RESTRAINT] Error uploading
    /var/log/messages. Seeing error Bad Request [soup_http_error_quark, 400].
    This error occurs because restraint reports the number of bytes to send
    but then sends more as the file continues to grow.  Now we only send the
    number of bytes from the point the transmission began and ignore
    subsequent lines in the log as they are just extra noise.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1700886`: Restraint not uploading resultoutputfile.log
    when local watchdog expires. The variable OUTPUTFILE was not
    being set.  It is now set to the tasks current.log (ref: 1609330) so
    it is now reported.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1730617`: Multihost: Task execution synchronization
    does not work in restraint. As documented in Beaker's Multihost Tasks
    section, Task 1 on both server and client must complete before moving
    on to Task 2 and so on.  A new plugin `85_sync_multihost_tasks` was
    added to cause synchronization between client and server tasks.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1700915`: Resolve inconsistency of MAXTIME vs MAX_TIME
    variables.  To resolve confusion, `RSTRNT_MAX_TIME` is being deprecated
    with an existing variable `KILLTIMEOVERRIDE`. This changeset documents
    this deprecation.
  | (Contributed by Tomas Klohna)
* | NEW: :bug:`1700926`: Allow task to adjust local watchdog.  The command
    rstrnt-adjust-watchdog only affects the external watchdog.  To be
    compatible with beah, this commmand also works for the local watchdog.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1705223`: Incomplete doc in regards to metadata/testinfo.desc.
    This is a spinoff from BZ1120496 but for restraint.  This changeset
    identified and documented variables in metadata and testinfo file.
  | (Contributed by Carol Bouchard)

Restraint 0.1.39
----------------

Released 27 February 2019.

* | NEW: :bug:`1552199`: Restraint-client now supports changing
    timeout value for the request.
  | (Contributed by Martin Styk)
* | FIXED: :bug:`1670377`: Fixed compilation issues for GCC9/Automake.
  | (Contributed by Martin Styk)

Restraint 0.1.38
----------------

Released 29 January 2019.

* | FIXED: :bug:`1670111`: Fixed crash of Restraint for ppc64le and aarch64
    architecture.
  | (Contributed by Bill Peck)

Restraint 0.1.37
----------------

Released 11 January 2019.

* | NEW: :bug:`1665390`: Added feature to set family from client XML.
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
