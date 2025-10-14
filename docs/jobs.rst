Jobs
====

Restraint parses a sub-set of the Beaker job XML [#]_. Here is an example
showing just the elements required for running in the stand-alone configuration.

::

 <job>
  <recipeSet>
   <recipe>
    <task name="/kernel/performance/fs_mark" keepchanges="yes">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/performance/fs_mark" />
     <params>
      <param name="foo" value="bar"/>
     </params>
    </task>
       .
       .
       .
    <task name="/kernel/foo/checker">
     <rpm name="rh-tests-kernel-foo-checker" path="/mnt/tests/kernel/foo/checker"/>
    </task>
   </recipe>
  </recipeSet>
 </job>

Naming Tasks
------------

For reporting purposes it is a good idea to name your tasks. For git tasks we
have settled on a standard where we use the sub-directory path from our git repo
as the task name. You can see that in the following example.

::

 <task name="/kernel/performance/fs_mark">

This name will be used when reporting on the status of the task and when
reporting results.

Task Roles
----------

Restraint supports role assignment for tasks or whole recipes for use in
multi-host jobs.

::

 <job>
  <recipeSet>
   <recipe role="SERVERS">
    <task name="/kernel/filesystems/nfs/connectathon-mh">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/filesystems/nfs/connectathon-mh" />
    </task>
   </recipe>
   <recipe>
    <task name="/kernel/filesystems/nfs/connectathon-mh" role="CLIENTS">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/filesystems/nfs/connectathon-mh" />
    </task>
   </recipe>
  </recipeSet>
 </job>

The above example results in environment variables "SERVERS" and "CLIENTS"
containing hostnames assigned to corresponding recipes. The variables will be
available only to tasks with the same padding within recipes.

Recipe roles function as default roles for tasks that have no role specified
and can be overridden by task roles.

Apart from role env variables Restraint also exports 2 more hostname-related
variables:

* RECIPE_MEMBERS - contains hostnames of all hosts within current recipeSet.
* JOB_MEMBERS - contains hostnames of all hosts in current job.

Keeping Your Task Changes Intact
--------------------------------

By default Restraint will fetch tasks every time you run a recipe overwriting
any changes you've done locally. This is not desirable in some cases, e.g. when
debugging a test. Restraint provides the ability to keep local changes by
setting task property "keepchanges" to "yes" in the job xml.

::

 <task name="/kernel/performance/fs_mark" keepchanges="yes">


Installing Tasks
----------------

The above example shows that you can install tasks directly from git or from an
RPM in a yum repo.

.. _fetch-label:

Fetch
~~~~~

The first example shows fetching a task from git.

::

 <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/performance/fs_mark" />

 OR

 <fetch ssl_verify="off" url="https://fedorapeople.org/cgit/bpeck/public_git/tests.git/snapshot/tests-master.tar.gz#kernel/performance/fs_mark" />

 OR

 <fetch recipe_abort_on_fail="true" url="https://fedorapeople.org/cgit/bpeck/public_git/tests.git/snapshot/tests-master.tar.gz#kernel/performance/fs_mark" />

The fetch node accepts git URI's that conform to the following:

* Prefixed with git:// OR use tarballs with http:// and cgit can serve them
  automatically.
* The fully qualified hostname. Remember that the system running restraintd must
  be able to reach this host.
* The path to the git repo.
* Optionally you can specify a valid reference which can be a branch, tag or
  SHA-1. ie: ?master
* Optionally you can specify a sub-dir. Restraint will only extract this sub-dir
  and run the task from here. ie: #kernel/performance/fs_mark. Notice that
  there is not a preceding slash here.
* If you need to disable SSL certificate checking you can set ssl_verify
  parameter to "off".
* If you need to abort the whole recipe on a fetch failure you can set
  recipe_abort_on_fail parameter to "true".

Restraint uses git's archive protocol to retrieve the contents so make sure
your git server has enabled this. You can enable this on most servers by
putting the following in your git repo config

::

 [daemon]
        uploadarch=true

RPM
~~~

The second example will attempt to install the task via yum/rpm.

::

 <rpm name="rh-tests-kernel-foo-checker" path="/mnt/tests/kernel/foo/checker"/>

Currently Restraint does not attempt to set up any repos that you may have
specified in your job.xml. This means that in order for it to install the
above task you must have already configured the task repo on the machine
running restraintd.

The path attribute tells restraint where the task scripts are installed.

Parameters
----------

You can optionally pass parameters to a task via environment variables. The
following snippet from our example would create an environment variable named
'foo' with the value 'bar'.

::

 <params>
  <param name="foo" value="bar"/>
 </params>

The parameter KILLTIMEOVERRIDE allows you to specify a different max time than
what is specified in the tasks metadata. KILLTIMEOVERRIDE is provided for
compatibility with legacy RHTS (Red Hat Test System).

As of 0.1.40, the parameter RSTRNT_MAX_TIME has been deprecated in favor of KILLTIMEOVERRIDE
because of confusion with RSTRNT_MAXTIME

The parameter RSTRNT_USE_PTY allows you to either enable or disable using a pty
for task execution. Use ``true`` to enable and ``false`` to disable. Setting
this value in the job will override the settings in metadata or testinfo.desc.

.. [#] `Beaker Job XML <http://beaker-project.org/docs/user-guide/job-xml.html>`_.
