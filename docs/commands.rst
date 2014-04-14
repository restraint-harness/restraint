Commands
========

* restraint - Used for stand alone execution

Use the restraint command to run a job on a remote test machine running
restraintd.  You can run them on the same machine but it is not recommended
since some tasks reboot the system.  

::

 restraint --remote http://addressOfMyTestSystem.example.com:8081 --job /path/to/job.xml

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

* rstrnt-report-result - Report Pass/Fail/Warn, optional score

Reporting plugins can be disabled by passing the plugin name to the --disable
option.  Here is an example of reporting a result but disabling the built in avc checker::

 rstrnt-report-result --disable 10_avc_check $RSTRNT_TASKNAME/sub-result PASS 100

Multiple plugins can be disabled by passing in multiple --disable arguments.

To stay compatible with legacy RHTS tasks, restraint also looks to see if
the environment variable AVC_ERROR is set to +no_avc_check.  If this is
true then it's the same as the above --disable 10_avc_check argument.

* rstrnt-report-log - Upload a log or some other file
* rstrnt-reboot - helper to reboot the system. On UEFI systems it will use efibootmgr to set next boot to what is booted currently.
* rstrnt-backup - helper to backup a config file
* rstrnt-restore - helper to restore a previously backed up file
* check_beaker - run from init/systemd, will run a beaker job

If you have the restraint-rhts subpackage installed these commands are provided in order to support legacy tests written for RHTS

* rhts-reboot - Use rstrnt-reboot instead
* rhts-backup - Use rstrnt-backup instead
* rhts-restore - Use rstrnt-restore instead
* job2html.xml - an xslt template to convert the stand alone job.xml results into a html doc.

Here is an example to convert a job run xml into an html doc.  This html doc can 
be easily navigated with a browser to investigate results and logs.

::

 xsltproc job2html.xml simple_job.07/job.xml > simple_job.07/index.html
