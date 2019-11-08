Commands
========

restraintd
----------

restraintd is the daemon which executes the tasks.

Both a SysV init script and a systemd unit file are provided. The included
spec file will use the correct one when built on RHEL/Fedora based systems.

Logging
~~~~~~~

All messages from restraintd will be printed to stderr and all output from
executing commands will be printed to stdout.

stderr is redirected to /dev/console to help debug when things
go wrong. The SysV init script will redirect both stdout + stderr to
/var/log/restraintd.log. For systemd you can use the journalctl command::

 journalctl --unit restraintd

 -- Logs begin at Fri 2014-04-11 16:39:13 EDT, end at Fri 2014-04-11 16:46:36 EDT. --
 Apr 11 16:40:20 virt-test systemd[1]: Starting The restraint harness....
 Apr 11 16:40:20 virt-test systemd[1]: Started The restraint harness..
 Apr 11 16:40:20 virt-test restraintd[567]: Waiting for client!
 Apr 11 16:40:20 virt-test restraintd[567]: * Fetching recipe: http://beaker.example.com:8000//recipes/1079/
 Apr 11 16:40:21 virt-test restraintd[567]: ** (restraintd:567): WARNING **: Ignoring Server Running state
 Apr 11 16:40:21 virt-test restraintd[567]: * Parsing recipe
 Apr 11 16:40:21 virt-test restraintd[567]: * Running recipe
 Apr 11 16:40:21 virt-test restraintd[567]: ** Fetching task: 1562 [/mnt/tests/distribution/install]
 Apr 11 16:40:27 virt-test restraintd[567]: Resolving Dependencies
 Apr 11 16:40:27 virt-test restraintd[567]: --> Running transaction check
 Apr 11 16:40:27 virt-test restraintd[567]: ---> Package beaker-distribution-install.noarch 0:1.10-15 will be installed
 Apr 11 16:40:30 virt-test restraintd[567]: --> Finished Dependency Resolution
 .
 .
 .
 Apr 11 16:40:36 virt-test restraintd[567]: Installed:
 Apr 11 16:40:36 virt-test restraintd[567]: beaker-distribution-install.noarch 0:1.10-15
 Apr 11 16:40:36 virt-test restraintd[567]: Complete!
 Apr 11 16:40:36 virt-test restraintd[567]: ** Parsing metadata
 Apr 11 16:40:36 virt-test restraintd[567]: ** Updating env vars
 Apr 11 16:40:36 virt-test restraintd[567]: ** Updating watchdog
 Apr 11 16:40:37 virt-test restraintd[567]: ** Installing dependencies
 Apr 11 16:41:00 virt-test restraintd[567]: Nothing to do
 Apr 11 16:41:00 virt-test restraintd[567]: ** Running task: 1562 [/distribution/install]
 Apr 11 16:41:00 virt-test restraintd[567]: TASK_RUNNER_PLUGINS: /usr/share/restraint/plugins/task_run.d/10_bash_login
 /usr/share/restraint/plugins/task_run.d/15_beakerlib /usr/share/restraint/plugins/task_run.d/20_unconfined make run
 Apr 11 16:41:01 virt-test restraintd[567]: -- INFO: selinux enabled: trying to switch context...
 .
 .
 .
 Apr 11 16:41:12 virt-test restraintd[567]: *** Running Plugin: 98_restore
 Apr 11 16:41:12 virt-test restraintd[567]: Nothing to restore.
 Apr 11 16:41:12 virt-test restraintd[567]: ** Completed Task : 1562


restraint
---------

Used for stand-alone execution.

Use the restraint command to run a job on a remote test machine running
restraintd. You can run jobs on the local machine but it is not recommended
since some tasks reboot the system. Hosts are tied to recipe IDs inside the
job XML.

::

 restraint --host 1=addressOfMyTestSystem.example.com:8081 --job /path/to/simple_job.xml

Restraint will look for the next available directory to store the results in.
In the above example it will see if the directory simple_job.01 exists. If
it does (because of a previous run) it will then look for simple_job.02. It
will continue to increment the number until it finds a directory that doesn't
exist.

By default Restraint will report the start and stop of each task run like this::

 Using ./simple_job.07 for job run
 * Fetching recipe: http://192.168.1.198:8000/recipes/07/
 * Parsing recipe
 * Running recipe
 *  T:   1 [/kernel/performance/fs_mark                     ] Running
 *  T:   1 [/kernel/performance/fs_mark                     ] Completed: PASS
 *  T:   2 [/kernel/misc/gdb-simple                         ] Running
 *  T:   2 [/kernel/misc/gdb-simple                         ] Completed: PASS
 *  T:   3 [restraint/vmstat                                ] Running
 *  T:   3 [restraint/vmstat                                ] Completed

You can pass -v for more verbose output which will show every task reported.
If you pass another -v you will get the output from the tasks written to your
screen as well.

All of this information is also stored in the job.xml which in this case is
stored in the ./simple_job.07 directory.

rstrnt-report-result
--------------------

Report Pass/Fail/Warn, optional score.

Reporting plugins can be disabled by passing the plugin name to the --disable
option. Here is an example of reporting a result but disabling the built in
AVC (Access Vector Cache) checker::

 rstrnt-report-result --disable 10_avc_check $RSTRNT_TASKNAME/sub-result PASS 100

Multiple plugins can be disabled by passing in multiple --disable arguments.

To stay compatible with legacy RHTS (Red Hat Test System) tasks, Restraint also
looks to see if the environment variable AVC_ERROR is set to +no_avc_check. If
this is true then it's the same as the above ``--disable 10_avc_check``
argument.

rstrnt-report-log
-----------------

Upload a log or some other file.

rstrnt-reboot
-------------

Helper to reboot the system. On UEFI systems it will use efibootmgr to set next
boot to what is booted currently.

rstrnt-prepare-reboot
---------------------

Prepare the system for rebooting. Similar to rstrnt-reboot,
but does not actually trigger the reboot.

If machine is UEFI and has efibootmgr installed, sets BootNext to
BootCurrent and uses :envvar:`NEXTBOOT_VALID_TIME` to determine for
how long (in seconds) this value is valid. After the specified time,
BootOrder is reset to previous state. Default value for
:envvar:`NEXTBOOT_VALID_TIME` is 180 seconds.

Tasks can run this command before triggering a crash or rebooting
through some other non-standard means. For example::

    rstrnt-prepare-reboot
    echo c >/proc/sysrq-trigger

rstrnt-backup
-------------

Helper to backup a config file.

rstrnt-restore
--------------

Helper to restore a previously backed up file. There is a plugin which is
executed at task completion which will call this command for you.

.. _rstrnt-sync-block:

rstrnt-sync-block
---------------

Block the task until the given systems in this recipe set have reached
a certain state.  Use this command, along with `rstrnt-sync-set` to
synchronize between systems in a multihost recipe set.

::

    rstrnt-sync-block -s <state> [--timeout <timeout>] [--retry <time>] [--any] <fqdn> [<fqdn> ...]

For a more detailed guide on multihosting, refer to
`Beaker Multihost documentation <https://beaker-project.org/docs/user-guide/multihost.html>`__.

.. option:: -s <state>

   Wait for the given state. If this option is repeated, the command will
   return when any of the states has been reached. This option is required.

.. option:: --retry <time>

   `rstrnt-sync-block` sleeps inbetween check for machine(s) states.
    If you'd like increase or decrease the frequency of checks, you can alter
    sleep time using the option `retry`.  The default is 60 seconds.

.. option:: --timeout <timeout>

   Return a non-zero exit status after *timeout* seconds if the state has
   not been reached. By default no timeout is enforced and the command will
   block until either the given state is reached on all specified systems
   or the recipe is aborted by the local or external watchdog.

.. option:: --any

   Return when any of the systems has reached the given state. By default, this
   command blocks until *all* systems have reached the state.

.. describe:: <fqdn> [<fqdn> ...]

   FQDN of the systems to wait for. At least one FQDN must be given. Use the
   role environment variables to determine which FQDNs to pass.

.. _rstrnt-sync-set:

rstrnt-sync-set
---------------

Sets the given state for this system. Other systems in the recipe set can use
`rstrnt-sync-block` to wait for a state to be set on other systems. The
syntax for this command is as follows:

::

    rstrnt-sync-set -s STATE

States are scoped to the current task. That is, states set by the current task
will have no effect in subsequent tasks.

On execution of the first `set` operation, a background process `rstrnt-sync`
is spawned which collects these states and responds to block requests.  This
server listens for events received on `TCP port 6776`.  All subsequent `set`
and `block` operations are forwarded to the `rstrnt-sync` server by way of
this socket.

This script also writes the states to the file named `/var/lib/restraint/rstrnt_events`.
This file is used when the system reboots enabling the states to be restored.

rstrnt-adjust-watchdog
----------------------

This command allows you to adjust both the external watchdog (if running with beaker)
and the local watchdog.  The time provided with the command replaces the current
watchdog time and does not add to or remove from current watchdog time.  This time can be
configured in seconds, minutes, and hours as
similarly described as ``TestTime`` metadata in
`Beaker User Guide <https://beaker-project.org/docs/user-guide/task-metadata.html>`_.
Once set, it will take up to ``HEARTBEAT`` (1 minute) time for the local watchdog
thread to wake up and see the changes (provided the metadata ``no_localwatch``
is false); however, the effective time is as soon as the command is executed since
current time is captured.  The external watchdog is increased by
``EWD_TIME`` (30 minutes) from the time you provide while the local watchdog
uses the exact time provided.

The following log entries appear in the harness.log file as watchdog's
heartbeat progresses every minute.::

*** Current Time: Fri May 17 15:15:49 2019 Localwatchdog at: Fri May 17 15:15:59 2019

When a user runs this command, you can expect to see the following log entry once
the change is first recognized.  Notice it is prefixed with 'User Adjusted'.
Also notice in this example the expire time is less than current time.  This can
occur if the command was run with number of seconds less than 1 minute.  There is a
delay waiting for the watchdog thread to wake up to handle the changes.  The thread
can recognize a change occurred at a previous point in time and will expire
the task immediately if the expired time is earlier than now.::

*** Current Time: Fri May 17 15:15:49 2019 User Adjusted Localwatchdog at: Fri May 17 15:15:02 2019

If the command is run with time less than the ``HEARTBEAT`` time, the following
warning will appear when the command is executed::

    Expect up to a 1 minute delay for watchdog thread to notice change.

If the task metadata has ``no_localwatchdog`` set to ``true``, the
local watchdog time is not adjusted with this new time.  However,
the external watchdog will continue to be adjusted. The log file
will show the following warning when this occurs::

    Adjustment to local watchdog ignored since 'no_localwatchdog'
    metadata is set

check_beaker
------------

Run from init/systemd, will run a Beaker job.

job2html.xml
------------

An XSLT (eXtensible Stylesheet Language Transformations) template to convert
the stand-alone job.xml results file into an HTML doc. The template can be
found in Restraint's ``client`` directory.

Here is an example command to convert a job run XML file into an HTML doc.
This HTML doc can be easily navigated with a browser to investigate results and
logs.

::

 xsltproc job2html.xml simple_job.07/job.xml > simple_job.07/index.html

job2junit.xml
-------------

An XSLT template to convert the stand-alone job.xml file into JUnit results.
The template can be found in Restraint's ``client`` directory.

Here is an example command to covert a job run XML into JUnit results.

::

 xsltproc job2junit.xml simple_job.07/job.xml > simple_job.07/junit.xml

Legacy RHTS Commands
--------------------

If you have the restraint-rhts subpackage installed these commands are provided
in order to support legacy tests written for RHTS.

rhts-reboot
~~~~~~~~~~~

Use `rstrnt-reboot` instead.

rhts-backup
~~~~~~~~~~~

Use `rstrnt-backup` instead.

rhts-restore
~~~~~~~~~~~~

Use `rstrnt-restore` instead.

rhts-environment.sh
~~~~~~~~~~~~~~~~~~~

Deprecated.

rhts-lint
~~~~~~~~~

Deprecated - only provided so that testinfo.desc can be generated.

rhts-run-simple-test
~~~~~~~~~~~~~~~~~~~~

Deprecated.

rhts-sync-set
~~~~~~~~~~~~~

Use `rstrnt-sync-set` instead.

rhts-sync-block
~~~~~~~~~~~~~~~

Use `rstrnt-sync-block` instead.

