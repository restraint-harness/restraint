===============================
Restraint (The Simple Harness)
===============================

Restraint is a simple harness that can be run standalone or as an alternate harness in beaker [#]_.
When using beaker it uses the alternate harness api [#]_ to fetch the recipe to execute and to report back results.
The recipe that restraint parses is stored as an XML document.  The structure is documented at the beaker web site [#]_.  The main advantage to using restraint is that it can retrieve tasks directly from git.  Other advantages are:

 - Can be staticaly linked to make it easier to test the system without changing the system.
 - Allows multiple clients to connect and watch test execution in real time.
 - Can be run stand alone without beaker.
 - Provides a pseudo tty and login shell for tasks.
 - Easier to debug than beaker's current harness beah.

.. [#] `Beaker <http://beaker-project.org>`_ is open-source software for managing and automating labs of test computers.
.. [#] `Alternate Harness <http://beaker-project.org/docs/alternative-harnesses>`_ API.
.. [#] `Job XML <http://beaker-project.org/docs/user-guide/job-xml.html>`_.

Building
=================

restraint can be built and linked dynamically or statically. To build it dynamically you will need the following packages installed (Minimum versions are listed):

 - zlib-1.2.8
 - bzip2-1.0.6
 - libffi-3.0.11
 - glib-2.38.0
 - libxml2-2.9.0
 - libarchive-3.1.2
 - xz-5.0.4
 - libsoup-2.42.2
 - sqlite-autoconf-3080002
 - intltool-0.35.5

Build restraint::

 % make

To build it statically first enter the third-party directory and build the support libraries::

 % pushd third-party
 % make
 % popd

Then build restraint with the following command::

 % pushd src
 % PKG_CONFIG_PATH=third-party/tree/lib/pkgconfig make STATIC=1
 % popd

Installing restraint::

 % make install

Running Standalone
==================

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
       <task name="restraint/vmstat">
         <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git#restraint/vmstat"/>
         <params>
           <param name="count" value="10"/>
         </params>
       </task>
     </recipe>
   </recipeSet>
 </job>

The format of the git URI is git://PATH_TO_GIT_REPO?(BRANCH or TAG or SHA1)#PATH_INSIDE_OF_REPO.  In the above example the first and last task retrieve the same code.  The first one specifies the branch master which is the default if you don't specify.  This is where it makes it real easy to test a change to a task without breaking the currently
deployed task.

Start the restraintd daemon::

 # service restraintd start

Tell restraint to run a job::

 % restraint --job /path/to/job.xml

You probably don't want to run restraintd on the machine you use for day to day activity.
Some tests can be destructive or just make unfriendly changes to your system.  restraint allows you
to run tasks on a remote system.  This means you can have the task git repo on your development
workstation and verify the results on your test system.  In order for this to work your git repo
and the recipe xml need to be accessible to your test system.  Here is an example::

 % restraint --remote http://addressOfMyTestSystem.example.com:8081 --job /path/to/job.xml

This will connect to restraintd running on addressOfMyTestSystem.example.com and tell it to run the recipe
from this machine.  Also remember that the tasks which are referenced inside of the recipe
need to be accessible as well. Here is the output::

 restraint --remote http://addressOfRemoteSystem:8081/ --job simple_job.xml -v
 Using ./simple_job.07 for job run
 *  T:   1 [/kernel/performance/fs_mark                     ] Running
 **      1 [Default                                         ] PASS
 **      2 [Random                                          ] PASS
 **      3 [MultiDir                                        ] PASS
 **      4 [Random_MultiDir                                 ] PASS
 *  T:   1 [/kernel/performance/fs_mark                     ] Completed: PASS
 *  T:   2 [/kernel/misc/gdb-simple                         ] Running
 **      5 [/kernel/misc/gdb-simple                         ] PASS Score: 0
 *  T:   2 [/kernel/misc/gdb-simple                         ] Completed: PASS
 *  T:   3 [restraint/vmstat                                ] Running
 *  T:   3 [restraint/vmstat                                ] Completed

All results will be stored in the job run directory which is 'simple_job.07' for this run.
In this directory you will find 'job.xml' which has all the results and references to all the task logs.
You can convert this into html with the following command::

 % xsltproc job2html.xml simple_job.07/job.xml >simple_job.07/index.html

jobs2html.xml is found in restraint's rpm doc directory.

Running in Beaker
=================

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
