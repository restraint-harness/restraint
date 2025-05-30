Processes and Commands
======================

There are two main restraint processes.  The first is the restraint server named
`restraintd` which processes tasks.  The second process supports restraint standalone.
This process is the restraint client named `restraint` which starts `restraintd`, provides
the job.xml information to the server, and collects logs and results from the server.

.. _restraintd_intro:

restraintd
----------

`restraintd` is the daemon which executes the tasks.

Both a SysV init script and a systemd unit file are provided. The included
spec file will use the correct one when built on RHEL/Fedora based systems.

Logging messages from restraintd are printed to stderr and all output from
command execution is printed to stdout.

stderr is redirected to /dev/console to help debug when things
go wrong. The SysV init script redirects both stdout and stderr to
/var/log/restraintd.log.  For systemd, use the following `journalctl`
command to view restraint logs::

 journalctl --unit restraintd

 -- Logs begin at Thu 2020-03-12 11:45:05 EDT, end at Thu 2020-03-12 12:10:47 EDT. --
 Mar 12 11:45:26 virt-test systemd[1]: Starting The restraint harness....
 Mar 12 11:45:26 virt-test systemd[1]: Started The restraint harness..
 Mar 12 11:45:26 virt-test restraintd[1135]: recipe: * Fetching recipe: http://lc.example.net:8000//recipes/30220/
 Mar 12 11:45:26 virt-test restraintd[1135]: Listening on http://localhost:8081
 Mar 12 11:45:26 virt-test restraintd[1135]: recipe: * Parsing recipe
 Mar 12 11:45:26 virt-test restraintd[1135]: recipe: * Running recipe
 Mar 12 11:45:26 virt-test restraintd[1135]: ** Fetching task: 183853 [/mnt/tests/distribution/check-install]
 Mar 12 11:45:26 virt-test restraintd[1135]: use_pty:FALSE rstrnt-package reinstall beaker-core-tasks-distribution-check-install
 Mar 12 11:45:32 virt-test yum[1194]: Installed: beaker-core-tasks-distribution-check-install-1.0-2.noarch
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Preparing metadata
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Refreshing peer role hostnames: Retries 0
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Updating env vars
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Updating external watchdog: 2400 seconds
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Installing dependencies
 Mar 12 11:45:33 virt-test restraintd[1135]: ** Running task: 183853 [/distribution/check-install]
 ...
 Mar 12 11:45:43 virt-test restraintd[1135]: ** Completed Task : 183853

When `restraintd` runs as a system service by SysV init or systemd, it
listens on the port 8081.

`restraintd` can also be paired with the restraint client at which case it does not run as
a service. More details on `Standalone` can be found at :ref:`restraint_client`.
In this case, any `restraintd` stdout/stderr output is directed to the `restraint`
client output.

The scripts and programs associated with the `restraintd` server can be
run within the context of a job as well outside a job execution.

.. _common-cmd-args:

Command Usage
~~~~~~~~~~~~~

Restraint commands are communicated to the running `restraintd` service
by providing a URL that `restraintd` is listening to.  When the
command is run within a job, the needed information is available by
way of environment variables set by `restraintd` for each task. When
the command is executed outside a job, you can provide the information
by one of three options. One option relies on setting of environment
variables. Second is the server option which requires you gather restraint
server port, recipe number, and task number for constructing the
command URL. Lastly is a local option which relies on an
environment file created by `restraintd`.

**Environment Variables Option**

Most often, many of restraint commands are executed in tasks included in your 'job.xml'.
As a result, commands look for specific environment variables to be set by restraintd.
The variables are as follows with data such as port, recipe, and task id which is
unique for each job::

    HARNESS_PREFIX=RSTRNT_
    RSTRNT_URL=http://localhost:<port>
    RSTRNT_RECIPE_URL=http://localhost:<port>/recipes/<recipe_id>
    RSTRNT_TASKID=<task_id>

.. note::
   <port> is a numeric value representing the port used to communicate with `restraintd`.
   <recipe_id> and <task_id> are the numeric values assigned to your running jobs recipe and task.

To utilize the environment variables option when executing a command outside your job, the command
software will default to look for environment variables when other `Server` and `Local` options
are not set.  These environment variables must be set by the user before executing the
command.

**Server Option**

The server option of calling these commands without exporting environment variables is to
provide the argument::

    -s, --server <server-url>

The format of the <server-url> is one of the following depending on the command::

    # for rstrnt-abort
      http://localhost:<port>/recipes/<recipe_id>/status/
    # for rstrnt-adjust-watchdog
      http://localhost:<port>/recipes/<recipe_id>/watchdog/
    # for rstrnt-report-results
      http://localhost:<port>/recipes/<recipe_id>/tasks/<task_id>/results/
    # for rstrnt-report-log. The string /logs/$file is appended by the command for you.
      http://localhost:<port>/recipes/<recipe_id>/tasks/<task_id>

**Local Option**

A simpler option is to run the command locally on the host running `restraintd` by
specifying the following argument::

    --port <server-port-number>

This option can be used on the same host running `restraintd` since the
information is derived from the local file
`/var/lib/restraint/rstrnt-commands-env-<$port>.sh` (where `$port` is the
port number restraintd listens on).  As the server progresses through a job,
it defines this file based on the current task. As a result, the user does not
need to gather recipe number and task number and construct a URL for a
command as this will be generated for you.  The port number must be
provided by the user.  For `restraintd` service, the default port of 8081 can
be used. When running with `restraint` client, the port number can be found
in `restraint` client log output since `restraintd` output is redirected
to the client.  Log locations for service and non-service `restraintd`
can be found in the section :ref:`restraintd_intro`.  The following log entry
is the one which contains the port number of interest::

  Listening on http://localhost:<port-number>

This `--port` option has similar effect to doing the following prior to executing the command::

    export $(cat /var/lib/restraint/rstrnt-commands-env-$port.sh)

In conclusion, one of three methods must be used to execute your command.
The following are examples of each method using the command `rstrnt-abort` as an example::

    rstrnt-abort                                                               # Environment Variables method
    rstrnt-abort -s http://localhost:<port>/recipes/<rid>/tasks/<tid>/status/  # Legacy Method
    rstrnt-abort --port <port>                                                 # Local Method

.. note::
   1. Replace <port>, <rid>, <tid> with your restraint port number, recipe id, task id.
   2. Given these fields change as the job progresses and if you are running the command
      outside the job, the window of opportunity to target the current running task is reduced
      when using the --port option.

rstrnt-abort
~~~~~~~~~~~~
Running this command sets a recipe to `Aborted` status. As a result, the current
task as well as subsequent tasks in the recipe will be marked as `aborted` and the job is discontinued.

Arguments for this command are as follows::

    rstrnt-abort [ --port <server-port-number> ] \
                   -s, --server <server-url>
                 ]

Where:

.. option:: --port <server-port-number>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

.. option:: -s, --server <server-url>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

   Where <server-url> is as follows::

       http://localhost:<port>/recipes/<recipe_id>/status/

rstrnt-adjust-watchdog
~~~~~~~~~~~~~~~~~~~~~~

This command allows you to adjust both the external watchdog and the local watchdog.

The arguments for this command is as follows::

    rstrnt-adjust-watchdog [ --port <server-port-number>] \
                             -s, --server <server-url>
                           ] <time>

Where:

.. option:: --port <server-port-number>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

.. option:: -s, --server <server-url>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

   Where server-url is `http://localhost:<port>/recipes/<recipe_id>/watchdog/`

.. option:: time

   This is a required argument.  This time can be configured in seconds, minutes, and hours.
   The value of the field should be a number followed by either the letter “m” or “h” to
   express the time in minutes or hours. It can also be specified in seconds by giving just
   a number. In most cases, it is recommended to provide a value in at least minutes rather
   than seconds.

   For example: 90 = 90 seconds, 1m = 1 minute, 2h = 2 hours

   The time should be the absolute longest a test is expected to take on the slowest
   platform supported, plus a 10% margin of error. Setting the time too short may lead to
   spurious cancellations, while setting it too long may waste lab system time if the task
   does get stuck. Durations of less than one minute are not recommended, as they usually run
   some risk of spurious cancellation, and it’s typically reasonable to take a minute to abort
   the test after an actual infinite loop or deadlock.

The time provided with the command replaces the current watchdog time as opposed to adding
to or removing from the current watchdog time.  Once set, it will take up to ``HEARTBEAT``
(1 minute) time for the local watchdog thread to wake up and see the changes (provided
the metadata ``no_localwatch`` is false); however, the effective time is as soon as the
command is executed since current time is captured.  The external watchdog is increased
by ``EWD_TIME`` (30 minutes) from the time you provide while the local watchdog
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

    Adjustment to local watchdog ignored since 'no_localwatchdog' metadata is set

.. _rstrnt-backup:

rstrnt-backup
~~~~~~~~~~~~~

Provides the ability to backup a list of files.  This command works in
concert with :ref:`rstrnt-restore` which restores the files.  In order
to preserve permissions and attributes of the files, it is recommended
to run this command as root. The command line for this features is as follows::

    rstrnt-backup [list of files to backup]

Other than the list of files to backup, there are no arguments with this
command. However, there exists an environment variable which may be used::

    RSTRNT_BACKUP_DIR - Specify an environment variable which can be set if you want
                        your files backed up in a directory other than default.
                        The default is in the subdirectory `/backup`.

.. _rstrnt-package:

rstrnt-package
~~~~~~~~~~~~~~

This command supports installation, removal, and re-installation of packages for
various OS package managers.  The restraintd server uses the command to perform
package operations for user's task `dependencies`.  It may be necessary for
user tasks to control these operations as part of their tests.

The arguments for this command are as follows::

    rstrnt-package  <install | remove | reinstall> <package-name>

The following are environment variables available to control execution of
this command::

    RSTRNT_PKG_CMD:      To specify which package manager command to use.
                         default: yum
    RSTRNT_ARG_ARGS:     To provide arguments to package manager command.
                         default: -y
    RSTRNT_PKG_INSTALL:  Specify package manager install operation.
                         default: install
    RSTRNT_PKG_REMOVE:   Specify package manager remove operation.
                         default: remove.
    RSTRNT_PKG_RETRIES:  Number of times to retry package operation.
                         default: 5
    RSTRNT_PKG_DELAY:    Number of seconds to delay between retries.
                         default: 1

.. _p_reboot:

rstrnt-prepare-reboot
~~~~~~~~~~~~~~~~~~~~~

Prepare the system for rebooting. Similar to rstrnt-reboot,
but does not actually trigger the reboot.

If machine is UEFI and has efibootmgr installed, sets BootNext to
BootCurrent and uses :envvar:`NEXTBOOT_VALID_TIME` to determine for
how long (in seconds) this value is valid. After the specified time,
BootNext setting is cleared so BootOrder takes precedence. Default
value for :envvar:`NEXTBOOT_VALID_TIME` is 180 seconds. To prevent
the clearing of BootNext, set :envvar:`NEXTBOOT_VALID_TIME` to
0 seconds.

Tasks can run this command before triggering a crash or rebooting
through some other non-standard means. For example::

    rstrnt-prepare-reboot
    echo c >/proc/sysrq-trigger

No arguments are required to run this command.


rstrnt-reboot
~~~~~~~~~~~~~

Helper to soft reboot the system. On UEFI systems, it will use efibootmgr
to set next boot to what is booted currently.  No arguments are required to run
this command.

rstrnt-report-log
~~~~~~~~~~~~~~~~~
The command `rstrnt-report-log` loads a log file for a given task. If called
multiple times for the same filename for the same task, it replaces the
previously sent file.

The arguments for this command are as follows::

    rstrnt-report-log [ --port <server-port-number> \
                        -s, --server <server-url> \
                      ] -l, --filename <logfilename>

Where:

.. option:: --port <server-port-number>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

.. option:: -s, --server <server-url>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

   Where `server-url` is `http://localhost:<port>/recipes/<recipe_id>/tasks/<task_id>`
   `rstrnt-report-log` completes the urls by appending `logs/$file` to your server-url.

.. option:: -l, --filename <logfilename>

   Specify the name of log file to upload.  This is a
   required argument.

rstrnt-report-result
~~~~~~~~~~~~~~~~~~~~

The command `rstrnt-report-result` sends a result report and alters the
status of the task.  This command can be called multiple times for a
single task each concluding with their own status results.   At conclusion
of the task, the final task result is the most severe rating. So if you
call the command with FAIL, then WARN, then PASS, or SKIP, the task status
results in FAIL.

This program runs in two modes.  One provides backward compatibility to
legacy harness and libraries and the other is restraint specific.
In the latter case, there are more features.  Both modes report a
result file, test results, and an optional score.

Restraint Reporting Mode
""""""""""""""""""""""""

For restraint reporting mode (not --rhts), the format of arguments is as follows::

    rstrnt-report-result [--port <server-port-number>] \
                          -s, --server <server-url> \
                          -o, --outputfile <outfilename> \
                          -p, --disable-plugin <plugin-name> --no-plugins] \
                         TESTNAME TESTRESULT [METRIC]
                         ]

Where:

.. option:: --port <server-port-number>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

.. option:: -s, --server <server-url>
   :noindex:

   Refer to :ref:`common-cmd-args` for details.

   Where `server-url` is `http://localhost:<port>/recipes/<recipe_id>/tasks/<task_id>/results/`

.. option:: -o, --outputfile <outfilename>

   Specify the name of file to upload.  If not specified, the
   environment variable $OUTPUTFILE is used if available.

.. option:: -p, --disable-plugin <plugin-name(s)>

   Disables the specified reporting plugins (see :ref:`rpt_result`)
   with the provided name or list of names. For example, to
   disable the built-in AVC (Access Vector Cache) checker, this
   argument would look like::

       --disable 10_avc_check

.. option:: --no-plugins

   Disables all reporting plugins

.. option::  TESTNAME

   Testname of the task. This is a required argument.

.. option::  TESTRESULT

   Indicates results of job.  It can be one of SKIP|PASS|WARN|FAIL (listed by
   increasing severity).  The highest severity received for a task becomes the
   final task result. The only result type that may need further explanation
   is SKIP. SKIP is useful when conditions on the device does not apply to
   this test. The task can be skipped and marked as such.  This
   allows the user the flexibility to use the same job for multiple hardware
   types, or OSs, or architectures, etc and omit tasks when not applicable.
   This is a required argument.

.. option::  METRIC

    Optional result metric

.. _legacy_rpt_mode:

Legacy Reporting Mode
"""""""""""""""""""""
The rhts extension of restraint uses --rhts.  The command line would appear as follows::

    rstrnt-report-result --rhts TESTNAME TESTRESULT LOG/OUTPUTFILE [METRIC]

Where:

.. option::  TESTNAME
   :noindex:

   Testname of the task. This is a required argument.

.. option::  TESTRESULT
   :noindex:

   Indicates results of job.  It can be one of SKIP|PASS|WARN|FAIL (listed by
   increasing severity).  The highest severity received for a task becomes the
   final task result. The only result type that may need further explanation
   is SKIP. SKIP is useful when conditions on the device does not apply to
   this test. The task can be skipped and marked as such.  This
   allows the user the flexibility to use the same job for multiple hardware
   types, or OSs, or architectures, etc and omit tasks when not applicable.
   This is a required argument.

.. option::  LOGFILE

   Output name of file. If not specified, the
   environment variable $OUTPUTFILE is used if available.

.. option::  METRIC
   :noindex:

    Optional result metric

The legacy mode depends on environment variables being defined as described in
:ref:`common-cmd-args`.  The options `-s, --server` and `--port` are not
supported for legacy mode.

Legacy mode looks to see if the environment variable AVC_ERROR is set
to +no_avc_check. If this is true, then its behavior is equivalent to the
non-legacy mode ``--disable 10_avc_check`` argument.

.. _rstrnt-restore:

rstrnt-restore
~~~~~~~~~~~~~~

Provides the ability to restore a previously backed up file(s). This command
works in concert with :ref:`rstrnt-backup` which performs the back up step.
There is a plugin which is executed at task completion which calls this command
for you (:ref:`completed` restore plugin).

.. _rstrnt-sync-block:

rstrnt-sync-block
~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~

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

This script also writes the states to the file named `/etc/restraint/rstrnt_events`.
This file is used when the system reboots enabling the states to be restored.

.. _restraint_client:

restraint
---------

The `restraint` client is used for standalone execution.

Use the `restraint` command to spawn a `restraintd` process to run a job on a
remote test machine.  You can run jobs on the local machine but it is not
recommended since some tasks reboot the system. Hosts are tied to recipe IDs
inside the job XML.

Arguments for the client are as follows::

    restraint --host <recipe-id>=<host> --job <job.xml> [--restraint-path </dir/restraintd>] [-v]

Where:

.. option:: --host <recipe_id>=[<user>@]<host>

   Set host for a recipe with specific id.  The recipe_id identifies which host
   correlates to the recipe with the same recipe id in your job.xml file. This is
   very useful for multihost testing.  If there is no id in the recipe of your
   job.xml file, then 1 is the default.

.. option:: --job </yourdir/your-job.xml>

   File Location of your job.xml.

.. option:: --restraint-path </dir/restraintd>

   The optional argument ``--restraint-path`` specifies the path to the restraintd
   binary to run on the remote machine.  This can be used by developers where the
   restraint repo is pulled and ``restraintd`` image is built.  By default, the
   installed image is executed.


.. option:: --timeout <minutes>
   :noindex:

   This optional argument ``--timeout`` specifies the time in minutes for
   ssh to timeout.  This option takes affect when the `rsh` argument is not
   used. The default timeout is 5 minutes.  A keepalive message is sent every
   minute to the server and this is done for the number of minutes provided.
   If there is no response, the ssh client will disconnect.

.. option:: -v

   You can pass ``-v`` for more verbose output which will show every task
   reported.  If you pass another ``-v`` you will get the output from the tasks
   written to your screen as well.

.. option:: -e, --rsh <command>

    You can pass ``-e/--rsh`` and define command that will be used to connect
    restraint client to restraintd.
    Default value is ``ssh -o ServerAliveInterval=60 -o ServerAliveCountMax=5``.
    Value ``ServerAliveCountMax`` is controlled by ``--timeout`` option mentioned above.

A sample of restraint command line is as follows:

.. code-block:: console

 restraint --host 1=addressOfMyTestSystem.example.com --job /path/to/simple_job.xml --restraint-path /home/userid/restraint/src/restraintd

.. end

By default, the `restraintd` launched in the remote system will randomly
choose a free port to listen on. The option ``-p, --port <port>`` can be
used to specify the port where `restraintd` will listen on.

Restraint will look for the next available directory to store the results in.
In the above example, it will see if the directory simple_job.01 exists. If
it does (because of a previous run) it will then look for simple_job.02. It
will continue to increment the number until it finds a directory that doesn't
exist.

By default, Restraint will report the start and stop of each task run like this::

 Using ./simple_job.07 for job run
 * Fetching recipe: http://localhost:42640
 * Parsing recipe
 * Running recipe
 *  T:   1 [/kernel/performance/fs_mark                     ] Running
 *  T:   1 [/kernel/performance/fs_mark                     ] Completed: PASS
 *  T:   2 [/kernel/misc/gdb-simple                         ] Running
 *  T:   2 [/kernel/misc/gdb-simple                         ] Completed: PASS
 *  T:   3 [restraint/vmstat                                ] Running
 *  T:   3 [restraint/vmstat                                ] Completed


All of this information is also stored in the job.xml which in this case is
stored in the ./simple_job.07 directory.

job2html.xml
~~~~~~~~~~~~

An XSLT (eXtensible Stylesheet Language Transformations) template to convert
the stand-alone job.xml results file into an HTML doc. The template can be
found in Restraint's ``client`` directory.

Here is an example command to convert a job run XML file into an HTML doc.
This HTML doc can be easily navigated with a browser to investigate results and
logs.

::

 xsltproc job2html.xml simple_job.07/job.xml > simple_job.07/index.html

job2junit.xml
~~~~~~~~~~~~~

An XSLT template to convert the stand-alone job.xml file into JUnit results.
The template can be found in Restraint's ``client`` directory.

Here is an example command to covert a job run XML into JUnit results.

::

 xsltproc job2junit.xml simple_job.07/job.xml > simple_job.07/junit.xml

Legacy RHTS Commands
--------------------

Prior to the `Restraint` harness, users used `RHTS` commands in their jobs.
These are being deprecated and substitutes for those legacy commands can be
found in :ref:`legacy_rhts_cmds`.
