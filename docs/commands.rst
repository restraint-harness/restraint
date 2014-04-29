Commands
========

restraintd
----------

restraintd is the daemon which executes the tasks.

Both a SysV init script and a systemd unit file are provided.  The included
spec file will use the correct one when built on RHEL/Fedora based systems.

Logging
~~~~~~~

All messages from restraintd will be printed to stderr and all output from
executing commands will be printed to stdout.

stderr is redirected to /dev/console to help debug when things
go wrong.  The SysV init script will redirect both stdout + stderr to 
/var/log/resatraintd.log.  For systemd you can use the journalctl command::

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

Used for stand alone execution

Use the restraint command to run a job on a remote test machine running
restraintd.  You can run them on the same machine but it is not recommended
since some tasks reboot the system.  

::

 restraint --remote http://addressOfMyTestSystem.example.com:8081 --job /path/to/simple_job.xml

Restraint will look for the next available directory to store the results in.
In the above example it will see if the directory simple_job.01 exists.  If
it does, because of a previous run, it will then look in simple_job.02.  It 
will continue doing this until it finds a directory that doesn't exist.

By default restraint will report the start and stop of each task run like this

::

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

But all of this information is stored in the job.xml which in this case is 
stored in ./simple_job.07.

rstrnt-report-result
--------------------

Report Pass/Fail/Warn, optional score

Reporting plugins can be disabled by passing the plugin name to the --disable
option.  Here is an example of reporting a result but disabling the built in avc checker::

 rstrnt-report-result --disable 10_avc_check $RSTRNT_TASKNAME/sub-result PASS 100

Multiple plugins can be disabled by passing in multiple --disable arguments.

To stay compatible with legacy RHTS tasks, restraint also looks to see if
the environment variable AVC_ERROR is set to +no_avc_check.  If this is
true then it's the same as the above --disable 10_avc_check argument.

rstrnt-report-log
-----------------

Upload a log or some other file

rstrnt-reboot
-------------

helper to reboot the system. On UEFI systems it will use efibootmgr to set next boot to what is booted currently.

rstrnt-backup
-------------

helper to backup a config file

rstrnt-restore
--------------

helper to restore a previously backed up file.  There is a plugin which is executed
at task completion which will call this command for you.

rstrnt-adjust-watchdog
----------------------

If you are running in Beaker this allows you to adjust the external watchdog.  This does not
modify the localwatchdog, so its usually only useful to tasks that have no_localwatchdog set
to true in their task metadata.

check_beaker
------------

run from init/systemd, will run a beaker job

job2html.xml
------------

an xslt template to convert the stand alone job.xml results into a html doc.

Here is an example to convert a job run xml into an html doc.  This html doc can 
be easily navigated with a browser to investigate results and logs.

::

 xsltproc job2html.xml simple_job.07/job.xml > simple_job.07/index.html

Legacy RHTS Commands
--------------------
If you have the restraint-rhts subpackage installed these commands are provided in order to support legacy tests written for RHTS

rhts-reboot
~~~~~~~~~~~

Use rstrnt-reboot instead

rhts-backup
~~~~~~~~~~~

Use rstrnt-backup instead

rhts-restore
~~~~~~~~~~~~

Use rstrnt-restore instead

rhts-environment.sh
~~~~~~~~~~~~~~~~~~~

deprecated

rhts-lint
~~~~~~~~~

deprecated - only provided so that testinfo.desc can be generated

rhts-run-simple-test
~~~~~~~~~~~~~~~~~~~~

deprecated
