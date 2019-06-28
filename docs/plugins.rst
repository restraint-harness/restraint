Plugins
=======

Restraint relies on plugins to execute tasks in the correct environment and to
check for common errors or simply to provide additional logs for debugging
issues. Here is a typical outline of how plugins are executed::

 run_task_plugins
  \
   10_bash_login
   |
   15_beakerlib
   |
   20_unconfined
   |
   25_environment
   |
   make run
   |\
   | report_result
    \
     report_result

 run_task_plugins
  \
   10_bash_login
   |
   15_beakerlib
   |
   20_unconfined
   |
   25_environment
   |
   run_plugins <- completed.d
   \
    98_restore


The report_result commands above cause the following plugins to be executed::

 run_task_plugins
  \
   05_linger
   |
   10_bash_login
   |
   15_beakerlib
   |
   20_unconfined
   |
   25_environment
   |
   30_restore_events
   |
   35_oom_adj
   |
   run_plugins <- report_result.d
    \
     01_dmesg_check
     |
     10_avc_check
     |
     20_avc_clear

These plugins do not run from the task under test. They run from restraintd
process. This allows for greater flexibility if your task is running as a
non-root user since a non-root user would not be able to inspect some logs and
wouldn't be able to clear dmesg log.

Task Run
--------

Task run plugins are used to modify the environment under which the tasks will
execute. Simply place the executable in /usr/share/restraint/task_run.d. The
list of files in this directory will be passed to exec in alphabetical order.

Restraint currently ships with two task run plugins:

* 05_linger - Enables session bus for user that Restraint is running as. You
  can disable this with RSTRNT_DISABLE_LINGER=1
* 10_bash_login - invoke a login shell.
* 15_beakerlib - Sets env vars to tell beakerlib how to report results in
  Restraint.
* 20_unconfined - if selinux is enabled on system run task in unconfined
  context.
* 25_environment - Will attempt to guess certain variables if they weren't
  defined, (OSARCH, OSMAJOR, etc..).
* 30_restore_events - Restores Multi-host states after a reboot.
* 35_oom_adj - sets the oom score low so we are less likely to be killed.

So the above plugins would get called like so::

 exec 05_linger 10_bash_login 15_beakerlib 20_unconfined 25_environment 30_restore_events 35_oom_adj "$@"

In order for this to work the task run plugins are required to exec "$@" at the
end of the script. Although task run plugins can't take any arguments they can
make decisions based on environment variables.

It should be pointed out that the task run plugins are executed for all other
plugins! This is to ensure plugins run with the same environment as your task.
When executed under all other plugins the following variable will be defined::

 RSTRNT_NOPLUGINS=1

You can do conditionals based on this so lets create a plugin which will start
a TCP capture::

 # Capture tcpdump data from every task
 cat << "EOF" > /usr/share/restraint/plugins/task_run.d/30_tcpdump
 #!/bin/sh -x

 echo "*** Running PLugin: $0"

 # Don't run from PLUGINS
 if [ -z "$RSTRNT_NOPLUGINS" ]; then
   tcpdump -q -i any -q -w $RUNPATH/tcpdump.cap 2>&1 &
   echo $! > $RUNPATH/tcpdump.pid
 fi

 exec "$@"
 EOF
 chmod a+x /usr/share/restraint/plugins/task_run.d/30_tcpdump

Report Result
-------------

Every time a task reports a result to Restraint these plugins will execute.

* 01_dmesg_check - This plugin checks dmesg output for the following failure
  strings.

::

 Oops|BUG|NMI appears to be stuck|Badness at

But then it runs any matches through an inverted grep which removes the
following:

::

 BIOS BUG|DEBUG|mapping multiple BARs.*IBM System X3250 M4

This is an effort to reduce false positives. Both of the above strings can be
overridden from each task by passing in your own `FAILURESTRINGS` or `FALSESTRINGS`
environment variables. This can be done for each task.

If you want all tasks in a recipe to use the same `FAILURESTRINGS` or `FALSESTRINGS`,
you could start your recipe with a task which creates the following files respectively:

::

  /usr/share/rhts/failurestrings
  /usr/share/rhts/falsestrings

When configuring the files, each string should be on a separate line instead of
separated with '|'.  For example, failurestrings would contain something like the
following:

::

  Oops
  BUG
  NMI appears to be stuck
  Badness at

The order of precedence for these variables/files is follows::

  1) Task environment variable
  2) User defined files
  3) and Hardcoded defaults as defined earlier in this section.

FAILURESTRINGS and FALSESTRINGS are processed separately so you could
define failure strings as an environment variable while maintaining
hardcoded defaults for false strings.

In some cases, the kernel will produce a multi-line error message (including
hardware information and stack trace) in the dmesg output which is delimited by
a "cut here" line at the beginning and an "end trace" line at the end. This
plugin will capture the entire contents of the multi-line trace and considers
it as a single failure. The FALSESTRINGS pattern is applied to the whole trace
to check for false positives.

* 10_avc_check - This plugin searches for AVC (Access Vector Cache) errors that
  have occurred since the last time a result was reported.
* 20_avc_clear - This moves the time stamp used by avc_check forward so that we
  don't see the same AVC's reported again, some tests might generate AVC's on
  purpose and disable the check but you will still want to move the time stamp
  forward.

If you need to skip error checking, refer to RSTRNT_DISABLED as described
in the environment variable section (see :ref:`env-variables`).

Local Watchdog
--------------

These plugins will only be executed if the task runs beyond its expected time
limit. Restraint currently ships with three plugins:

* 10_localwatchdog - uploads the resultoutputfile.log of the running task.
* 20_sysinfo - Collects and uploads system information.

  * Uploads system log which contains a collection of system information
    such as slabinfo, list of blocked tasks derived from ``sysrq m, t and w``,
    and pre-existing system log messages.  Depending if journalctl exists,
    file journalctl or /var/log/messages is uploaded.
  * Uploads ps-lwd.log which contains a verbose list of running processes.
  * Uploads dmesg log if it contains any output.
  * Uploads user logs listed in $TESTPATH/logs2get.

* 99_reboot - Simply reboots the system to try and get the system back to a
  sane state.  If you need to skip this step, you can use RSTRNT_DISABLED
  as described in (see :ref:`env-variables`).

Completed
---------

These plugins will get executed at the end of every task, regardless if the
localwatchdog triggered or not. The only plugin currently shipped with
Restraint is:

* 98_restore - any files backed up by either rhts-backup or rstrnt-backup will
  be restored.

To finish our tcpdump example from above we can add the following::

 #Kill tcpdump and upload
 cat << "EOF" > /usr/share/restraint/plugins/completed.d/80_upload_tcpdump
 #!/bin/sh -x

 kill $(cat $RUNPATH/tcpdump.pid)
 rstrnt-report-log -l $RUNPATH/tcpdump.cap
 EOF
 chmod a+x /usr/share/restraint/plugins/completed.d/80_upload_tcpdump

If you need to skip file restoration, refer to RSTRNT_DISABLED as described
in the environment variable section (see :ref:`env-variables`).

