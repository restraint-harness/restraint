Tasks
=====

Restraint doesn't require tasks to be written in any particular language.  In fact,
most tests are written in a mixture of shell, python and c code.  You do need to provide
some metadata in order for things to work best.

Metadata
--------

Restraint will look for a file called metadata in the task directory.  The format
for that file is a simple ini file which most people should be familiar with.

::

 [General]
 name=/restraint/env/metadata
 owner=Bill Peck <bpeck@redhat.com>
 description=just reports env variables
 license=GPLv2
 confidential=no
 destructive=no

 [restraint]
 entry_point=./runtest.sh
 max_time=5m
 dependencies=gcc;emacs
 use_pty=False

The "General" section is mostly used for informational purposes.  The only element
that restraint will read from here is the name attribute.  If defined this will over write
the task name specified from the job xml.

The "restraint" section has the following elements which can be defined:

entry_point
~~~~~~~~~~~

This tells restraint how it should start running the task.  If you don't specify a program to run it will default to make run which is what legacy RHTS would do. Other examples of entry points:

* entry_point=autotest-local control-file
* entry_point=STAF local PROCESS START SHELL COMMAND "ps | grep test | wc >testcount.txt"

max_time
~~~~~~~~

The maximum time a task is expected to run.  When restraintd runs a task it sets up a localwatchdog
which will kill the task after this time has expired.  When run in beaker this is also used for the 
external watchdog (typically 20-30 minutes later than the local watchdog time).  Time units can be
specified as follows:

* d for days
* h for hours
* m for minutes
* s for seconds

To set a max run time for 2 days you would use the following

::

 max_time = 2d

dependencies
~~~~~~~~~~~~

A semicolon-delimited (``;``) list of additional packages (needed to run this
task) to be installed on the system.

::

 dependencies=lib-virt;httpd;postgresql;nfs-utils;net-tools;net-snmp;ethereal;wireshark;tcpdump;rusers;bzip2;gcc

repoRequires
~~~~~~~~~~~~

A semicolon-delimited (``;``) list of additional tasks needed for this task to run.

::

 repoRequires=general/include;filesystems/include

**Note:** When fetching from git (see :ref:`fetch-label`), this is the
``#subdirectory`` portion of the URL, so do *not* use a leading ``/`` character
as was done with RhtsRequires in testinfo.desc for Legacy RHTS tasks.

no_localwatchdog
~~~~~~~~~~~~~~~~

Normally restraint will setup a localwatchdog which will attempt to recover from a hung task 
before the external watchdog (if running under beaker) triggers.  But you can tell restraint to
not setup a localwatchdog monitor by including this key with a value of true. Only true or false are
valid values.

::

 no_localwatchdog=true

use_pty
~~~~~~~

Before version 0.1.24 restraint would execute all tasks from a pty.  This meant that programs
thought they were running in an interactive terminal and might produce ansi codes for coloring
and line postioning.  Now the default is not to use a pty which will give much cleaner output. 
If you find your test is failing because it expects a tty you can enable the old behavior by
setting this.

::

use_pty=True

OSMajor Specific options
~~~~~~~~~~~~~~~~~~~~~~~~

Any of the above elements can be overridden with OSMajor specific options.  In order for this to work
the OSMajor attribute must be filled in the job.xml.  If the job was run through beaker this will
have been filled in for you.  If you ran a stand alone job it is up to you to fill that value in correctly.

For example, if a task is known
to take twice as long on RedHatEnterpriseLinuxServer5 then you could use following

::

 max_time=5m
 max_time[RedHatEnterpriseLinuxServer5]=10m

Another example where we will install rhdb on RedHatEnterpriseLinuxServer5 and postgresql on everything else.

::

 dependencies=postgresql
 dependencies[RedHatEnterpriseLinuxServer5]=rhdb

testinfo.desc
-------------

Legacy RHTS tests use this file for their metadata [#]_.  Restraint supports generating (via the Makefile) and
reading this file.  But restraint does not understand all the fields in this file.  The following are the ones
restraint parses:

 * Name - Same as [General] name
 * TestTime - Same as [restraint] max_time
 * Requires - Same as [restraint] dependencies
 * USE_PTY - Same as [restraint] use_pty

Please see the beaker documentation for how to populate these fields.

.. [#] `RHTS Task Metadata <https://beaker-project.org/docs/user-guide/task-metadata.html>`_.
