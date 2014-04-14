Daemon
======

restraintd is the daemon which executes the tasks.

Both a SysV init script and a SystemD unit file are provided.  The included
spec file will use the correct one when built on RHEL/Fedora based systems.

Logging
-------

All messages from restraintd will be printed to stderr and all output from
executing commands will be printed to stdout.

stderr is redirected to /dev/console to help debug when things
go wrong.  The SysV init script will redirect both stdout + stderr to 
/var/log/resatraintd.log.  For SystemD you can use the journalctl command::

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
 Apr 11 16:41:00 virt-test restraintd[567]: TASK_RUNNER_PLUGINS: /usr/share/restraint/plugins/task_run.d/10_bash_login /usr/share/restraint/plugins/task_run.d/15_beakerlib /usr/share/restraint/plugins/task_run.d/20_unconfined make run
 Apr 11 16:41:01 virt-test restraintd[567]: -- INFO: selinux enabled: trying to switch context...
 .
 .
 .
 Apr 11 16:41:12 virt-test restraintd[567]: *** Running Plugin: 98_restore
 Apr 11 16:41:12 virt-test restraintd[567]: Nothing to restore.
 Apr 11 16:41:12 virt-test restraintd[567]: ** Completed Task : 1562


