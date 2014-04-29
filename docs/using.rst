Using Restraint
===============

Running in Beaker
-----------------

To use restraint in beaker you need to specify an alternate harness and include a repo where that harness can be
installed from::

 <recipe ks_meta="harness=restraint">
  <repos>
   <repo name="restraint"
         url="http://bpeck.fedorapeople.org/restraint/fc19/"/>
  </repos>
  .
  .
  .
 </recipe>

I have built restraint for multiple relases and arches at the above location but you will need to update the path
for the correct release.  You can always build your own repo of course.

If you have tasks/tests that were written for legacy RHTS you can install the restraint-rhts sub-package which
will bring in the legacy commands so that your tests will execute properly.  Some tasks/tests have also been 
written with beakerlib.  Here is an example recipe node that will install both for you::

 <recipe ks_meta="harness='restraint-rhts beakerlib'">
 .
 .
 .
 </recipe>

If you are using beaker command line workflows use these command line options::

 bkr <WORKFLOW> --ks-meta="harness=restraint" --repo http://bpeck.fedorapeople.org/restraint/fc19/

If you need RHTS compatibility and/or beakerlib you can add it here as well::

 bkr <WORKFLOW> --ks-meta="harness='restraint-rhts beakerlib'" --repo http://bpeck.fedorapeople.org/restraint/fc19/

Running Stand Alone
-------------------

Restraint can run on its own without beaker, this is handy when you are devleoping a test and would like quicker
turn around time.  Before restraint you either ran the test locally and hoped it would act the same when run
inside beaker or dealt with the slow turn around of waiting for beaker to schedule, provision and finally run
your test.  This is less then ideal when you are actively developing a test.

You still need a job xml file which tells restraint what tasks should be run.  Here is an example where we run three tests directly from git::

 <?xml version="1.0"?>
 <job>
   <recipeSet>
     <recipe>
       <task name="/kernel/performance/fs_mark">
         <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/performance/fs_mark"/>
       </task>
       <task name="/kernel/misc/gdb-simple">
         <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/misc/gdb-simple"/>
       </task>
       <task name="/kernel/standards/usex" role="None">
        <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git#kernel/standards/usex"/>
       </task>
     </recipe>
   </recipeSet>
 </job>

Tell restraint to run a job::

 % restraint --job /path/to/job.xml

You probably don't want to run restraintd on the machine you use for day to day activity.
Some tests can be destructive or just make unfriendly changes to your system.  restraint allows you
to run tasks on a remote system.  This means you can have the task git repo on your development
workstation and verify the results on your test system.  In order for this to work your git repo
and the recipe xml need to be accessible to your test system.  Be sure to have the restraint-client
package installed on the machine you will be running the restraint command from

Here is an example::

 % restraint --remote http://addressOfMyTestSystem.example.com:8081 --job /path/to/job.xml

This will connect to restraintd running on addressOfMyTestSystem.example.com and tell it to run the recipe
from this machine.  Also remember that the tasks which are referenced inside of the recipe
need to be accessible as well. Here is the output::

 restraint --remote http://addressOfRemoteSystem:8081/ --job simple_job.xml -v
 Using ./simple_job.07 for job run
 * Fetching recipe: http://192.168.1.198:8000/recipes/07/
 * Parsing recipe
 * Running recipe
 *  T:   1 [/kernel/performance/fs_mark                     ] Running
 **      1 [Default                                         ] PASS
 **      2 [Random                                          ] PASS
 **      3 [MultiDir                                        ] PASS
 **      4 [Random_MultiDir                                 ] PASS
 *  T:   1 [/kernel/performance/fs_mark                     ] Completed: PASS
 *  T:   2 [/kernel/misc/gdb-simple                         ] Running
 **      5 [/kernel/misc/gdb-simple                         ] PASS Score: 0
 *  T:   2 [/kernel/misc/gdb-simple                         ] Completed: PASS
 *  T:   3 [/kernel/standards/usex                          ] Running
 **  :   6 [/kernel/standards/usex                          ] PASS
 *  T:   3 [/kernel/standards/usex                          ] Completed: PASS

All results will be stored in the job run directory which is 'simple_job.07' for this run.
In this directory you will find 'job.xml' which has all the results and references to all the task logs.
You can convert this into html with the following command::

 % xsltproc job2html.xml simple_job.07/job.xml >simple_job.07/index.html

jobs2html.xml is found in restraint's rpm doc directory.

