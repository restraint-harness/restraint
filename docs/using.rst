Using Restraint
===============

Running in Beaker
-----------------

Beaker will use restraint by default if you are running Red Hat Enterprise Linux
version 8 or later or if you are running Fedora.

To use Restraint in Beaker for earlier versions of Red Hat Enterprise Linux or
Fedora, you will need to specify 'restraint' as the harness::

 <recipe ks_meta="harness=restraint">
 <repos>
  <repo name="restraint"
        url="https://beaker-project.org/yum/harness/CentOS7/"/>
 </repos>
  .
  .
  .
 </recipe>

If you have tasks/tests that were written for legacy RHTS (Red Hat Test System)
you can install the restraint-rhts sub-package which will bring in the legacy
commands so that your tests will execute properly. Some tasks/tests have also
been written with beakerlib. Here is an example recipe node that will install
both for you::

 <recipe ks_meta="harness='restraint-rhts beakerlib'">
 .
 .
 .
 </recipe>

If you are using Beaker command line workflows use these command line options::

 bkr <WORKFLOW> --ks-meta="harness=restraint" --repo https://beaker-project.org/yum/harness/CentOS7/

If you need RHTS compatibility and/or beakerlib you can add it here as well::

 bkr <WORKFLOW> --ks-meta="harness='restraint-rhts beakerlib'" --repo https://beaker-project.org/yum/harness/CentOS7/

.. _standalone:

Running Standalone
-------------------

Restraint can run on its own without Beaker, this is handy when you are
developing a test and would like quicker turn around time. Before Restraint you
either ran the test locally and hoped it would act the same when run inside
Beaker or dealt with the slow turn around of waiting for Beaker to schedule,
provision and finally run your test. This is less then ideal when you are
actively developing a test.

You still need a job XML file which tells Restraint what tasks should be run.
Here is an example where we run three tests directly from git::

 <?xml version="1.0"?>
 <job>
   <recipeSet>
     <recipe id="1">
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

Tell Restraint client to run a job:

.. code-block:: console

 restraint --job /path/to/job.xml

.. end

You probably don't want to run the restraintd server on the machine you use for day to day
activity. Some tests can be destructive or just make unfriendly changes to your
system. Restraint client allows you to run tasks on a remote system. This means you
can have the task git repo on your development workstation and verify the
results on your test system. In order for this to work your git repo and the
recipe XML need to be accessible to your test system. Be sure to have the
restraint-client package installed on the machine you will be running the
restraint client command from

Here is an example:

.. code-block:: console

    restraint --host 1=addressOfMyTestSystem.example.com --job /path/to/job.xml --restraint-path /home/userid/restraint/src/restraintd -v

.. end

This will spawn the restraintd server from the path specified in ``--restraint-path``
on host addressOfMyTestSystem.example.com and tell it to run the recipe with id="1" from
this machine. Also remember that the tasks which are referenced inside of the recipe
need to be accessible a well. Here is the output::

 restraint --host 1=addressOfRemoteSystem --job simple_job.xml --restraint-path /home/userid/restraint/src/restraintd -v
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

All results will be stored in the job run directory which is 'simple_job.07'
for this run. In this directory you will find 'job.xml' which has all the
results and references to all the task logs. You can convert this into HTML
with the following command:

.. code-block:: console

  xsltproc job2html.xml simple_job.07/job.xml >simple_job.07/index.html

.. end

``job2html.xml`` is found in Restraint's ``client`` directory.

Running in Beaker and Standalone
--------------------------------

Sometimes the tests that I am developing can be destructive to the system so I
don't want to run them on my development box. Or the test is specific to an
architecture so I can't use a VM for it on my machine. These are cases where
it's really handy to use a combination of Beaker for provisioning and
Standalone for executing the tests. By default, Beaker provides a test harness
for all imported distributions. You can replace test harness with your build
by adding a new repository. You can create your build on your own or you can
use different RPM build systems, for example COPR. Be aware that custom
restraint should have higher NVR than the latest released version and your
build needs to be built against distribution you planning to test. Otherwise,
DNF may pick up Restraint provided by Beaker or Restraint may fail to install.

First step is to run the following workflow to reserve a system in Beaker::

 <job><whiteboard>restraint reservesys</whiteboard>
  <recipeSet>
   <recipe ks_meta="harness=restraint" id="1">
    <distroRequires>
     <and>
        <distro_family op="=" value="Fedorarawhide"/>
        <distro_variant op="=" value="Everything"/>
        <distro_name op="=" value="Fedora-Rawhide-20200406.n.0"/>
        <distro_arch op="=" value="ppc64le"/>
     </and>
    </distroRequires>
    <hostRequires/>
    <repos>
     <repo name="my_custom_restraint" url="http://copr-be.cloud.fedoraproject.org/path/to/copr/repo/results"/>
    </repos>
    <task name="/distribution/check-install" role="STANDALONE" />
    <task name="/distribution/reservesys" role="None">
     <fetch url="https://github.com/beaker-project/beaker-core-tasks/archive/master.zip#reservesys"/>
    </task>
   </recipe>
  </recipeSet>
 </job>

This will reserve a ppc64 system running Fedora Rawhide. The /distribution/reservesys
task will email the submitter of the job when run so you know the system is
available. By default the reservesys task will give you access to the system
for 24 hours, after that the external watchdog will reclaim the system. You can
extend it using extendtesttime.sh on the system.

You can spawn a second instance of restraintd server using the client command below.  It will
generate an instance with a different port than the port used by beaker.

.. code-block:: console

 restraint --host 1=FQDN.example.com --job simple_job.xml --restraint-path /home/userid/restraint/src/restraintd -v

.. end

If you want to run restraint commands such as ``rstrnt-adjust-watchdog nn`` or
``rstrnt-abort`` against this test set-up, you must first export the environment
variables which includes the dynamically created communication port.  To do this, run
the following:

.. code-block:: console

 export $(cat /etc/profile.d/rstrnt-commands-env.sh)

.. end

If the task you are developing doesn't work as expected you can make changes
and try again. Just remember to push your changes to git, the system under test
will pull from the git URL you put in your job XML.
