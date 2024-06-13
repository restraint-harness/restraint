Release Notes
=============

Restraint 0.4.5
---------------

Bug Fixes
~~~~~~~~~

* | Improve task fetch for git repositories
  | To improve reliability when fetching tasks from git repositories, the number of retries has been changed from 3 to 30.
    To further increase the chance of success, the fetch interval has been changed from 10 seconds to 20 seconds.

Other Notable Changes
~~~~~~~~~~~~~~~~~~~~~

* | Dependency Updates
  | The following dependencies have been updated:

    * openssl: 1.1.1k -> 1.1.1w
    * json-c: 0.13.1 -> 0.16

Restraint 0.4.4
---------------

Bug Fixes
~~~~~~~~~

* | Fix: Stabilize selinux behavior in RHEL-9
  | Added a static selinux policy for RHEL-9.  The policy is manually
    built from a RHEL-9.0.0 host so it will work on all RHEL-9 versions.
    If built on a later version of RHEL-9, it will not be backward
    compatible with older versions and fail to install.

Restraint 0.4.3
---------------

Bug Fixes
~~~~~~~~~

* | Fix: Revert fix to fetch either branch 'main' or 'master'
  | When performing a `fetch` operation, restraint will continue
    to look for `master` branch only.

* | Fix: Revert Fetch URL fix
  | Backing out fix to provide precise directory matching for
    `repoRequires` and `fetch` operation. The problem is not all user
    selection data is precise.  Some user testing will be hindered
    with this change.  Due to these unknown cases, requesting users
    chose to instead make their directory definitions more precise.
    Refer to comment in Issue 272 on 3/13 for more details.
    If folks decide to reintroduce this change, they should also
    apply pull 289 which provides more flexibility.

Restraint 0.4.2
---------------

Bug Fixes
~~~~~~~~~

* | Fix: Fetch either branch 'main' or 'master'
  | When performing a `fetch` operation, restraint will look
    for either `main` or `master` branch.

* | Fix: rstrnt-reboot not reliable for UEFI systems
  | When `efibootmgr` is present, the `BootNext` variable is set to reboot
    to `Current`.  When `rstrnt-prepare-reboot` was written, a timer was set
    to remove `BootNext` setting after 180 seconds. `rstrnt-reboot`
    uses the `prepare` script and the timer wasn't long enough and not
    needed for `rstrnt-reboot`. This changeset allows `NEXTBOOT_VALID_TIME`
    to be set to 0.  When 0, the timer is not set and as a result
    `BootNext` will not be removed. `rstrnt-reboot` now uses a 0 timer.

* | Fix: Fetch URL extract too many matched directories
  | When `fetch` url is used, restraint is copying anything that
    matches the pattern in `https://<snip>#pattern` regardless
    of the location in the received path.  If pattern is `include`,
    both `general/include, include` directories will match when it
    should only be `include`.  Restraint will now only select if it
    matches starting from beginning of received path NOT throughout
    directory path. But first, the first `string/` must be ignored from
    the received path since it is superfluous for the match since it
    includes the repo and branch name added by curl.  Jobs that include
    this repo-branch prefix in the fetch pattern will now fail with this
    changeset.  So fetching `https://<snip>#repo-branch/pattern`
    will fail.

* | Fix: Use of FALSE/FAILURESTRINGS results in 'too many arguments'
  | Seeing the following errors in restraint.log files.
    `restraintd[2330]: ./01_dmesg_check: line 53: [: too many arguments`
    Added Quote around the value to prevent this.

Restraint 0.4.1
---------------

Other
~~~~~

 * | Tag `0.4.1` was intentionally skipped.

Restraint 0.4.0
---------------

Bug Fixes
~~~~~~~~~

* | Set OOMPolicy=continue to prevent killing restraintd (Fedora/RHEL9+)
  | Upon memory depletion, prevent the kernel from killing restraintd service.
    Keep restraintd running, but log the service event. This OOMPolicy setting
    is only available for RHEL9+ and Fedora distros.  Other distros will
    remain unchanged.

* | Prevent restraint client from being interrupted by SIGPIPE signal
  | Code change is to ignore SIGPIPE then client code will naturally
    attempt to retry once determined that the path is broken.

* | rstrnt-reboot to ignore SIGTERM
  | When this scripts performs a reboot, it does a loop forever to prevent
    from returning to the calling process.  This changeset ignores SIGTERMs
    to keep it from interrupting the loop.  We must keep the SIGKILL in place
    however so there is still a small window of opportunity of returning to
    caller but the window has been narrowed with this change.

Other Notable Changes
~~~~~~~~~~~~~~~~~~~~~

* | RHEL 5 and 6 builds are no longer created from this version onward.
  | This is partly due to upgrades of libraries used by restraint which
    are not compatible with the older RHEL releases. Older restraint releases
    for RHEL 5 and 6 prior to this 0.4.0 release will still exist in the
    download repositories.

Restraint 0.3.3
---------------

Bug Fixes
~~~~~~~~~

* | Restraint client honors job_id defined in <recipe> tag.

* | Improve error handling on recipe and task state management
  | Some errors that could indicate a bad saved state are now handled
    and reported.

* | Fix distro version check in 20_unconfined
  | Make it better at detecting Fedora 34 as Fedora and distinguish RHEL
    from Fedora in version comparison. The main difference is that it
    now picks the right SELinux context for test jobs on Fedora 34
    (Rawhide at the time of writing).

Restraint 0.3.2
---------------

What’s New
~~~~~~~~~~

* Enable log manager for log caching


Bug Fixes
~~~~~~~~~

* | Upload cached logs in time intervals
  | The log manager uploads the cached contents of taskout.log and
   harness.log every 15 seconds. This allows to follow task progress
   and avoids missing logs when the external watchdog aborts the
   recipe.

Restraint 0.3.1
---------------

Bug Fixes
~~~~~~~~~

* | Disable log manager
  | The log manager is disabled and the behavior for taskout.log and
   harness.log is the same as before it was introduced.

Restraint 0.3.0
---------------

What’s New
~~~~~~~~~~

* | Wait on Beaker's health status
  | When Restraint runs under Beaker, Beaker's health status is checked
    before performing steps that require communication with Beaker.
    Recipe execution is held until Beaker is available.
* | Log manager for log caching
  | When Restraint runs under Beaker, harness and task logs are cached
    in the system. Logs are uploaded to Beaker after the task completes.
  | Contributed by Ernestas Kulik <ernestask@gnome.org>


Bug Fixes
~~~~~~~~~

* | Recognize results reported for non-rhts tasks
  | When the task reports just `SKIP` for results, the final task result
    should be `SKIP`. An extra task result is occurring when a non-rhts task
    is executed.  An non-rhts task is one that uses the `metadata` file
    instead of `testinfo` file.  Bugzilla 1334893 made a change to always
    report results `PASS` for task exiting with zero or `FAIL` when
    exit non-zero for non-rhts tasks.  As a result, `PASS` was being
    reported which has a high priority then `SKIP` so the final task
    result was `PASS`.
  | Code changes monitor whether user reports results by way of
    `rstrnt-report-result`.  If so, give those results priority; otherwise,
    hardcode `PASS` task result for user.
  | When process exits with non-zero, `FAIL` for non-rhts will remain as this
    provides the user the option to continue with the job.  If they want
    legacy behavior, they should make a call to `rstrnt-abort` in their task.

* | Stop logging `LWD is disabled` every minute
  | When LWD (Local Watchdog) is disabled, there is a message in the
    harness log that reports this every minute.  The message looks
    like: `Localwatchdog at:  Disabled! `.  This changeset makes sure
    it is no longer reported repeatedly when `no_localwatchdog=true`
    is configured in the task `metadata` file.  To ensure there is some
    type of keepalive mechanism, the client now performs ssh keepalive
    towards the server.  This timeout value is configurable by use
    of the restraint client option `--timeout` which only affects default
    behavior. The timeout value has no effect when the `rsh` argument
    is used.

* | Use new task install default for non-RHTS package
  | For restraint-rhts package, tasks are installed and executed
    beneath `/mnt/tests`.  For non-rhts `restraint`
    installations, this path has changed to a more appropriate
    location.
  | The `20_sysinfo` plugin processes journalctl log in a temporary location
    instead of `/mnt` as it is just an interim event.


Restraint 0.2.3
---------------

Bug Fixes
~~~~~~~~~

- Fix noisy Restraint client output

  The Restraint client was not honoring the verbosity levels and it
  was printing all output available even when the verbosity level was
  the lowest.
  The Restraint client output now behaves similarly to release 0.1.45.

- Increase retries for recipe fetching

  Retries for recipe fetching are increased from 3 attempts in 10
  seconds intervals to 12 attempts in 10 seconds intervals.

Restraint 0.2.2
---------------

Bug Fixes
~~~~~~~~~

- restraint client now honors recipe params as well as task params.

- Correct commands exit status when argument parsing fails due to
  bad syntax. Commands always return non-zero in case of failure.

- Resolve loop in local watchdog plugin

  When the local watchdog (LWD) expires a task, the LWD plugin `20_sysinfo`
  goes into an infinite loop since the directory `/mnt/testarea` is not
  created for the non-rhts restraint package. An error returned by `diff`
  utility within an infinite loop was not anticipated.  The fix
  terminates the infinite loop when diff returns error.

Restraint 0.2.1
---------------

What's New 
~~~~~~~~~~
* | Add ability to select `restraintd` instance by port to `restraint` commands
  | When running commands outside of jobs on the local host,
    some `restraint` commands require manually setting up
    environment variables or constructing long URLs before running.
    This can be issue if you are trying to extend the watchdog in
    a timely fashion.  A new option was added which requires the
    argument `--port <restraint-port-number>`. Commands affected
    are `rstrnt-report-log`, `rstrnt-report-result`, `rstrnt-abort`, and
    `rstrnt-watchdog`.
* | Restore ability to specify `restraintd` port
  | Add the `-p, --port` option back to restraint daemon and client to specify the
    port where `restraintd` will be listening to. :bug:`1821342`
* | Document how to remove RHTS from Jobs
  | Added new section :ref:`rm_rhts_guide` detailing
    substitutes for `RHTS` scripts, environment variables,
    and testinfo.desc file and associated variables. :bug:`1802610`

Bug Fixes
~~~~~~~~~
* | Redirect task STDIN back to /dev/null
  | In release 0.2.0, the task STDIN was redirected to a pipe shared with the server. This
    breaks `ausearch` command when the input is not explicitly specified, as by default, if
    STDIN is a pipe, it will read from it, instead of system logs. As the pipe is closed
    when the task is running, tests expecting matches failed, and tests expecting no matches
    were unreliable.  Restoring redirect of task STDIN back to /dev/null ensures that `ausearch`
    reads from system logs by default.
* | Restore default port for restraind system service
  | In release 0.2.0 the port for `restraintd` system service is chosen dynamically,
    breaking workflows where the port was expected to persist between reboots.
    When `restraintd` runs as a system service, the port defaults to `8081`. :bug:`1823545`
* | Restraintd killed by SIGTRAP
  | It was discovered that an error logging function (g_error) introduced in 0.2.0 also
    performed aborts.  The function was replaced with one which logs without undesirable
    side effects. :bug:`1823840`, :bug:`1831824`
* | `restraintd` fails to start if both, IPv4 and IPv6, are not available on the loopback interface
  | In this release, restraintd will not fail if it's able to listen on at least one protocol,
    IPv4 or IPv6, although it will still try to listen on both.
* | Fix use of uninitialized FD for STDIN when PTY is requested
  | When PTY was requested, the FD for the task STDIN was left uninitialized. The value,
    set to 0, was still used in a close call, closing the parent STDIN FD and causing
    unexpected behavior in task execution.  In this release, the FD for STDIN is not used
    when PTY is requested.

Restraint 0.2.0
---------------

Upgrades
~~~~~~~~
* | :bug:`1667510`: Remove libssh from restraint client.
  | The port used by restraint server is no longer static.
    If using the restraint client, refer to restraint documentation
    for changes to arguments passed since the port is no longer
    included in `--host` argument. The client spawns restraintd
    for you so the extra step of starting up a restraintd instance
    is no longer needed. Because of these interface changes, the
    restraint client and server must be the same version.
  | (Contributed by Bill Peck and Carol Bouchard)
* | :bug:`1770230`: Replace rhts-sync- with rstrnt-sync- cmds.
  | This changeset creates rstrnt-sync- commands and links
    rhts-sync- commands to it. The multihost plugin now
    uses rstrnt-sync- commands.
  | (Contributed by Carol Bouchard)
* | :bug:`1802261`: Upgrade libxml2 to version 2.9.10
  | (Contributed by Daniel Rodriguez Gonzalez)

Bug Fixes
~~~~~~~~~
* | :bug:`1795915`: Remove execute permission from systemd service file.
    There is a warning message in the systemd logs about the file being
    executable.
  | (Contributed by John Villalovos)

Restraint 0.1.45
----------------

* | FIXED: :bug:`1795781`: Multihost sync hangs on remote reboot.
    Users multihost synchronization task hangs on block operation
    when remote host reboots.  This is a corner case difficult to
    reproduce.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1792466`: Restraint segfault during labcontroller timeout.
    On error when gathering peer roles from the lab controller, a double
    free of the error structure causes bad behavior in glib
    memory management.  Eventually this causes restraint server to crash
    on a segfault.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1691485`: Rstrnt Client not provide task vers in job.xml.
    This change affects rpm tasks only.  Restraint server gets the
    version number from the rpm and returns it in 'Completed/Aborted'
    status message sent to restraint client.  The restraint client
    writes it out in the job.xml.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1793114`: Wrong file permission on 30_dmesg_clear plugin.
    The new 30_dmesg_clear plugin does not have execute file permission.
    However, other scripts add execution permission so it is correct in
    the rpm.  This is being fixed in repo to prevent chasing it as
    an issue.
  | (Contributed by Carol Bouchard)

Restraint 0.1.44
----------------

* | FIXED: :bug:`1788252`: restraintd crash in timeout_callback functions.
    Ran into timing issues when process_timeout_callback occurs after
    process_pid_callback.  The task data is NULL so process_timeout_callback
    should not attempt to process task data when pid is 0 indicating
    process is complete.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1781722`: Not executing task when multihost utilized.
    Observed that restraint reported the task started but output from
    the task itself not making it to taskout.log file. With debug
    enabled, found it stopped in 30_restore_events plugin.
    Performed more detail unit testing on rstrnt-sync and resolved
    a number of issues found.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1782422`: Fetch https operation noisy harness.log.
    When using <fetch url="https://github.com/repo#dirname> in task, the
    entire repo is downloaded and a log entry for each file/dir found
    is logged.  These log entries get reported to Lab Controller
    which results in reduced performance.  Fixed code to report
    only entries found beneath the directory name 'dirname'.
  | (Contributed by Carol Bouchard)

Restraint 0.1.43
----------------

* | FIXED: :bug:`1774211`: Seeing too many repo extraction.
    Under certain conditions, restraint was failing to go
    to next repoRequires operation causing redundant
    fetch operations to occur.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1236568`: Separate dmesg clear from check.
    Need for a separate plugin so clear of the dmesg logs
    is done independently from check dmesg logs.
    Currently this is done during `dmesg check` plugin.
    If `dmesg check` plugin is disabled, so is the clear
    operation leaving the next task will process unrelated
    errors. By separating clear from check operation, the clear
    operation can always be performed.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1749316`: Rstrnt retry refresh role on socket io err.
    User periodically observed "Error: Socket I/O Timed out".
    This occurred during the restraint task state
    "** Refreshing peer role hostnames" which collects
    host roles from lab controller and there is no response
    in default 1 minute time frame.  To handle network
    issues, restraint will retry this event similar to
    what is done when performing fetch operations.
  | (Contributed by Carol Bouchard)
* | FIXED: :bug:`1762731`: Rstrnt add more metadata UTs.
  | (Contributed by Carol Bouchard)
* | NEW: :bug:`1455763`: New command rstrnt-prepare-reboot.
    It does the same preparatory work as rstrnt-reboot, but does not
    trigger the reboot. Tasks can use this prior to (intentionally)
    crashing the system or rebooting it in some other non-standard
    way.
  | (Contributed by Tomas Klohna)

Restraint 0.1.42
----------------

* | FIXED: :bug:`1753652`: Multihost Sync Improvements.
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
* | FIXED: :bug:`1756515`: FALSESTRINGS not provide consistent results.
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

* | FIXED: :bug:`1753336`: The cli rstrnt-adjust-watchdog command.
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
* | FIXED: :bug:`1751074`: Rlse 0.1.40 seeing a lot of invalid.
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
