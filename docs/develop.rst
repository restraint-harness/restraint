Developer Guide
===============

If you have questions related to Restraint's development that are not
currently answered in this guide, the two main ways to contact the
Restraint development team are the same as those for getting
general assistance with using and installing Beaker:

- the `development mailing list <https://lists.fedorahosted.org/mailman/listinfo/beaker-devel>`__
- the #beaker IRC channel on FreeNode

This document focuses on the mechanics of working with Restraint's
code base with the target audience being a Restraint user interested
in learning more about Restraint's working or a potential Restraint
contributor.

Getting Started
~~~~~~~~~~~~~~~

Restraint is written in C. The source lives in a git repo on
http://github.com/beaker-project/ along with other related projects. The
following creates a local clone of the Restraint source.

.. code-block:: console

    git clone git@github.com:beaker-project/restraint.git

.. end

Restraint uses a number of external libraries/tools, so before you can
build Restraint you need to install the external libraries using
``dnf builddep restraint.spec``. Once you have installed these dependencies,
running a ``make all`` at the source directory root will compile and build
restraint, restraintd, and commands. To also run a quick sanity check, it is
a good idea to run the unit tests using ``make check``. The unit tests use a
simple Python HTTP server and ``git-daemon``, so you will need to install
this as well
(``dnf install git-daemon``).

Testing Changes
~~~~~~~~~~~~~~~

If you have fixed an existing bug or implemented a new feature, it is
a good idea to add a relevant test. The existing tests can be found in
the ``src/`` directory in the source files with names starting with
``test_``.

It may also be a good idea to run a recipe by building the Restraint
daemon and client from the modified code base. You can build the
binaries using ``make all`` in the ``src`` directory.

From the same directory, run the restraint client with a reference to a job.xml.
The following shows how to initiate the restraint client to execute a recipe:

.. code-block:: console

    restraint --host 1=127.0.0.1 --job /path/to/job.xml --restraint-path /my_development_path/restraint/src/restraintd

.. end

Developers should use the option ``--restraint-path`` to point to the development path
of the restraintd server.  More details on this can be found in :ref:`standalone`.

Submitting a Patch
~~~~~~~~~~~~~~~~~~

All patches are submitted using GitHub Pull Requests feature.
To do that you have to have the fork of the Restraint.

Created patch should also have a note for release notes.
The note has to be created by `Reno <https://docs.openstack.org/reno/latest/user/usage.html>`__.
