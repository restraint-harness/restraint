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

 % PKG_CONFIG_PATH=third-party/tree/lib/pkgconfig make STATIC=1

Running Standalone
==================

Restraint can run on its own without beaker, this is handy when you are devleoping a test and would like quicker
turn around time.  Before restraint you either ran the test locally and hoped it would act the same when run 
inside beaker or dealt with the slow turn around of waiting for beaker to schedule, provision and finally run
your test.  This is less then ideal when you are actively developing a test.

You still need a recipe xml file which tells restraint what tests should be run.  Here is an example where we run three tests directly from git::

 <job>
  <recipeSet>
   <recipe job_id="123" arch="x86_64" distro="RHEL5-Server-U8" family="RedHatEnterpriseLinuxServer5" id="796557" recipe_set_id="648468" variant="">
    <task id="1" status="Waiting">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#restraint/vmstat" />
     <params>
      <param name="count" value="6"/>
     </params>
    </task>
    <task id="2" status="Waiting">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#restraint/sleep" />
    </task>
    <task id="3" status="Waiting">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git#restraint/vmstat" />
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

 % restraint --run file:///path/to/recipe.xml --monitor

You probably don't want to run restraintd on the machine you use for day to day activity.
Some tests can be destructive or just make unfriendly changes to your system.  restraint allows you
to run tasks on a remote system.  This means you can have the task git repo on your development
workstation and verify the results on your test system.  In order for this to work your git repo
and the recipe xml need to be accessible to your test system.  Here is an example::

 % restraint --server http://addressOfMyTestSystem.example.com:8081 --run http://addressOfThisSystem.example.com/recipe.xml --monitor

This will connect to restraintd running on addressOfMyTestSystem.example.com and tell it to run the recipe located
at addressOfThisSystem.example.com.  Also remember that the tasks which are referenced inside of the recipe
need to be accessible as well. Here is the output::

 * Fetching recipe: http://addressOfThisSystem.example.com/recipe.xml
 * Parsing recipe
 * Running recipe
 ** Fetching task: 1 [/mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat]
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat/metadata
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat/runtest.sh
 ** Parsing metadata
 ** Updating env vars
 ** Updating watchdog
 ** Installing dependencies
 ** Running task: 1 [/restraint/vmstat]
 + VMSTAT 10 6
 PROCS -----------MEMORY---------- ---SWAP-- -----IO---- -SYSTEM-- ----CPU----
  R  B   SWPD   FREE   BUFF  CACHE   SI   SO    BI    BO   IN   CS US SY ID WA
  0  1      0 581056  18316 253268    0    0   357    39   78  115  2  2 92  4
  0  0      0 580932  18360 253264    0    0     0    24   42   39  0  1 96  3
  0  0      0 580932  18376 253264    0    0     0    13   12   12  0  0 97  2
  0  0      0 580932  18392 253264    0    0     0     8   13   11  0  0 98  2
  1  0      0 580932  18408 253264    0    0     0     8   12   12  0  0 98  2
  1  0      0 580808  18424 253264    0    0     0    13   12   12  0  0 98  2
 ** Completed Task : 1
 ** Fetching task: 2 [/mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/sleep]
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/sleep/metadata
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/sleep/runtest.sh
 ** Parsing metadata
 ** Updating env vars
 ** Updating watchdog
 ** Installing dependencies
 ** Running task: 2 [/restraint/sleep]
 + SLEEP 7200
 *** Current Time: Sat Dec 07 17:20:41 2013 Localwatchdog at: Sat Dec 07 17:30:41 2013
 *** Current Time: Sat Dec 07 17:25:41 2013 Localwatchdog at: Sat Dec 07 17:30:41 2013
 ** Completed Task : 2
 ** Fetching task: 3 [/mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat]
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat/metadata
 ** Extracting /mnt/tests/fedorapeople.org/home/fedora/bpeck/public_git/tests.git/restraint/vmstat/runtest.sh
 ** Parsing metadata
 ** Updating env vars
 ** Updating watchdog
 ** Installing dependencies
 ** Running task: 3 [/restraint/vmstat]
 + vmstat 10 10
 procs -----------memory---------- ---swap-- -----io---- -system-- ----cpu----
  r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa
  0  1      0 579636  18812 253280    0    0   156    19   40   57  1  1 96  2
  0  0      0 579908  18860 253280    0    0     0    26   46   33  0  1 94  5
  0  0      0 579908  18884 253280    0    0     0    54   14   15  0  0 96  4
  0  0      0 579916  18900 253280    0    0     0     9   13   13  0  0 98  2
  0  0      0 579916  18916 253280    0    0     0     8   12   11  0  0 97  2
  0  0      0 579916  18932 253280    0    0     0     8   12   12  0  0 98  2
  0  0      0 579792  18948 253280    0    0     0     9   12   11  0  0 98  2
  0  0      0 579792  18964 253280    0    0     0     8   12   11  0  0 98  2
  0  0      0 579792  18980 253280    0    0     0     8   12   12  0  0 98  2
  0  0      0 579792  18996 253280    0    0     0    10   13   11  0  0 98  2
 ** Completed Task : 3

 * Results Summary
 *  Task:            1 [/restraint/vmstat                                 ] Result: NONE Status: Completed
 *  Task:            2 [/restraint/sleep                                  ] Result: NONE Status: Aborted
 * Error: Local watchdog expired! Killed 1230 with 9                                      
 *  Task:            3 [/restraint/vmstat                                 ] Result: NONE Status: Completed
 One or more tasks failed [restraint-client-stream-error, 7]

Running in Beaker
=================

To use restraint in beaker you need to specify an alternate harness and include a repo where that harness can be
installed from::

 <recipe ks_meta="harness=restraint">
  <repos>
   <repo name="mylittleharness"
         url="http://example.com/restraintd/el6/" />
  </repos>
  .
  .
  .
 </recipe>

Currently restraint has not had a proper release yet, so we don't have a repo to install from.
