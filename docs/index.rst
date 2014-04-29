.. restraint documentation master file, created by
   sphinx-quickstart on Fri Jan 24 10:20:22 2014.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to restraint's documentation!
=====================================

Restraint is designed to execute tasks.  These tasks can be tests which
report results or simply code that you want to automate.
Which tasks to execute is determined by a job [#]_.  The job also describes
where to retrieve the tasks from and what parameters to pass in.  These 
tasks can report multiple PASS, FAIL, WARN results along with an optional
score.  Tasks also have the ability to report log files.  Each task can
have metadata describing dependencies and max run time for example.
Execution and reporting can be further enhanced with plugins.

Restraint can be used with Beaker [#]_ since it talks beaker's Harness API [#]_ for
reporting results.  But it can also be used stand alone.

Features
--------

* Tasks can be retrieved directly from git.
* Does not rely on Anaconda/kickstart to install test dependencies
* Can be statically linked to make it easier to test the system without changing the system.
* Can be run stand alone without Beaker.

  - Tasks are executed with the same environment (no surprises when run later in Beaker).
  - Developing tasks is much quicker since you don't have to build task rpms, schedule a system, provision a system, etc...

* Can be easily extended with Plugins.
* Uses Beaker's job xml

The following documentation will show you how to use restraint in both environments.

Contents:

.. toctree::
   :maxdepth: 2

   install
   commands
   jobs
   tasks
   variables
   plugins
   using
   todo

Additional Information
======================

.. [#] `Job XML <http://beaker-project.org/docs/user-guide/job-xml.html>`_.
.. [#] `Beaker <http://beaker-project.org>`_ is open-source software for managing and automating labs of test computers.
.. [#] `Alternate Harness <http://beaker-project.org/docs/alternative-harnesses>`_ API.

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

