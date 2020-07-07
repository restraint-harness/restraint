.. _rm_rhts_guide:

Guide to removing RHTS from Jobs
================================

For some products, Test Requirements include running restraint by itself.
This requires the exclusion of the legacy RHTS package or `restraint-rhts`
package installation.  Below lists areas to draw attention in order
to eliminate RHTS references.

#. Use the `restraint` harness package and not `restraint-rhts` in your jobs.
#. Avoid defining tasks or dependencies which cause installation of the `RHTS` library.
#. Replace `RHTS` scripts with `Restraint` scripts.  :ref:`legacy_rhts_cmds` provides
   a table which maps legacy to restraint scripts.
#. Change your tasks to utilize `Restraint's metadata` file instead of `RHTS`
   `testinfo.desc` file. :ref:`legacy_metadata` provides details on mapping
   legacy `testinfo.desc` variables to restraint `metadata` variables. Depending
   on how your task is written, you may have to update or remove `Makefiles` so they
   do not process the `testinfo.desc` file.  An example of this is also
   included in the referenced section.
#. Replace `RHTS` environment variables with `Restraint` variables. A table listing
   `RHTS Legacy Variables` to `Restraint Substitute` can be found in
   :ref:`legacy_env_var`.

.. _legacy_rhts_cmds:

Replacement for RHTS Scripts
----------------------------

The table below lists known legacy RHTS commands.  Some are provided in the
`restraint-rhts` package and some are from `rhts` package.  It is encouraged for people
to use `Restraint`'s substitute for these commands as they are actively supported.
Included in the table are the `Restraint` substitutes and which RHTS commands are deprecated.

+--------------------------------+-------------------------------------------+
| RHTS Legacy Script             | Restraint Substitute                      |
+================================+===========================================+
| rhts-abort                     | rstrnt-abort                              |
+--------------------------------+-------------------------------------------+
| rhts-backup                    | rstrnt-backup                             |
+--------------------------------+-------------------------------------------+
| rhts-db-submit-result          | rstrnt-report-result.d plugin             |
| rhts_db_submit_result          | :ref:`rpt_result` (See Note)              |
+--------------------------------+-------------------------------------------+
| rhts-environment.sh            | None                                      |
| rhts_environment.sh            |                                           |
+--------------------------------+-------------------------------------------+
| rhts-extend                    | rstrnt-adjust-watchdog                    |
+--------------------------------+-------------------------------------------+
| rhts-flush                     | None                                      |
+--------------------------------+-------------------------------------------+
| rhts-lint                      | None                                      |
+--------------------------------+-------------------------------------------+
| rhts-power                     | None                                      |
+--------------------------------+-------------------------------------------+
| rhts-reboot                    | rstrnt-reboot                             |
+--------------------------------+-------------------------------------------+
| rhts-recipe-sync-block         | rstrnt-sync-block                         |
| rhts_recipe_sync_block         |                                           |
+--------------------------------+-------------------------------------------+
| rhts-recipe-sync-set           | rstrnt-sync-set                           |
| rhts_recipe_sync_set           |                                           |
+--------------------------------+-------------------------------------------+
| rhts-report-result             | rstrnt-report-result                      |
+--------------------------------+-------------------------------------------+
| rhts-restore                   | rstrnt-restore                            |
+--------------------------------+-------------------------------------------+
| rhts-run-simple-test           | None                                      |
+--------------------------------+-------------------------------------------+
| rhts-submit-log                | rstrnt-report-log                         |
| rhts_submit_log                |                                           |
+--------------------------------+-------------------------------------------+
| rhts-sync-block                | rstrnt-sync-block                         |
| rhts_sync_block                |                                           |
+--------------------------------+-------------------------------------------+
| rhts-sync-set                  | rstrnt-sync-set                           |
| rhts_sync_set                  |                                           |
+--------------------------------+-------------------------------------------+
| rhts-system-info               | localwatchdog.d 20_sysinfo plugin         |
|                                | :ref:`lcl_wd_p_in` (See Note)             |
+--------------------------------+-------------------------------------------+


.. note::
    Some functionality in `RHTS` scripts are replaced by `Restraint` plugins.  Links
    for details on those plugins are contained in the `Restraint Substitute` column.

.. _legacy_metadata:

Replacement for RHTS testinfo.desc File
---------------------------------------

Legacy `RHTS` tests use the `testinfo.desc` file for their metadata [#]_. `Restraint`
supports generating (via the Makefile) and reading this file; however, `Restraint`
does not process all the fields in this file. `Restraint` gives the `metadata` file
predecence over the `testinfo.desc` file.

The tables below shows is a list of `testinfo.desc` variables `Restraint` parses
and acts on.  The table also shows the mapping to `Restraint` metadata
section/variable.

+------------------------+----------------------------------------+
| testinfo.desc variable | metadata [section] variable substitute |
+========================+========================================+
| Name                   | [General] name                         |
+------------------------+----------------------------------------+
| Environment            | [restraint] environment                |
+------------------------+----------------------------------------+
| TestTime               | [restraint] max_time                   |
+------------------------+----------------------------------------+
| Requires               | [restraint] dependencies               |
+------------------------+----------------------------------------+
| RhtsRequires           | [restraint] dependencies               |
+------------------------+----------------------------------------+
| USE_PTY                | [restraint] use_pty                    |
+------------------------+----------------------------------------+

The following are informational variables and should be maintained.
`Restraint` does not perform any action on these variables.

+------------------------+----------------------------------------+
| testinfo.desc variable | metadata [section] variable substitute |
+========================+========================================+
| License                | [General] license                      |
+------------------------+----------------------------------------+
| Owner                  | [General] owner                        |
+------------------------+----------------------------------------+
| Description            | [General] description                  |
+------------------------+----------------------------------------+
| Confidential           | [General] confidential                 |
+------------------------+----------------------------------------+
| Destructive            | [General] destructive                  |
+------------------------+----------------------------------------+

There are no substitutes for the following `Makefile/testinfo.desc` variables
in `Restraint's` metadata file.  Some of these variables are
informational and can be added in the metadata file but it is just
documentation.  `Restraint` will not act on them and they will be ignored.

* TESTVERSION
* FILES
* BUILT_FILES
* TEST_DIR
* Path
* Architectures
* Bugs
* Priority
* Releases
* RhtsOptions
* TestVersion

Example of removing testinfo.desc file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The sample files below show converting `Makefile/testinfo.desc` to `metadata` file.
The Makefile does not have to be removed in its entirety. In the Sample Makefile,
everything from `rhts-make.include` below should be removed.  If the
upper part of the Makefile is kept, the `entry_point` variable defined in the `metadata`
file is not required since `Restraint` will perform `make run` when `entry_point` is
not present.

Sample Makefile::

 export TEST=/examples/no-rhts/sample-before
 export TESTVERSION=1.0

 BUILT_FILES=

 FILES=$(METADATA) runtest.sh Makefile PURPOSE

 .PHONY: all install download clean

 run: $(FILES) build
         ./runtest.sh

 build: $(BUILT_FILES)
         test -x runtest.sh || chmod a+x runtest.sh

 clean:
         rm -f *~ $(BUILT_FILES)

 include /usr/share/rhts/lib/rhts-make.include

 $(METADATA): Makefile
         @echo "Owner:           User ABC1 <userabc1@example.com>" > $(METADATA)
         @echo "Name:            $(TEST)" >> $(METADATA)
         @echo "TestVersion:     $(TESTVERSION)" >> $(METADATA)
         @echo "Path:            $(TEST_DIR)" >> $(METADATA)
         @echo "Description:     Sample-before-no-rhts" >> $(METADATA)
         @echo "Type:            Sanity" >> $(METADATA)
         @echo "TestTime:        5m" >> $(METADATA)
         @echo "Priority:        Normal" >> $(METADATA)
         @echo "License:         GPLv2+" >> $(METADATA)
         @echo "Confidential:    no" >> $(METADATA)
         @echo "Destructive:     no" >> $(METADATA)
         @echo "Releases:        -RHEL7 -RHEL8" >> $(METADATA)
         @echo "Architectures:   x86_64" >> $(METADATA)

Makefile generated `testinfo.desc` file::

 Owner:           User ABC1 <userabc1@example.com>
 Name:            /examples/no-rhts/sample-before
 TestVersion:     1.0
 Path:            /mnt/tests/examples/no-rhts/sample-before
 Description:     Sample-before-no-rhts
 Type:            Sanity
 TestTime:        5m
 Priority:        Normal
 License:         GPLv2+
 Confidential:    no
 Destructive:     no
 Releases:        -RHEL7 -RHEL8
 Architectures:   x86_64

Replacement restraint `metadata` file with no Makefile::

 [General]
 description=Sample-after-no-rhts
 owner=User ABC1 <userabc1@example.com>
 license=GPLv2+
 confidential=no
 destructive=no

 [restraint]
 entry_point=./runtest.sh
 max_time=5m
 name=/examples/no-rhts/sample-after

.. _legacy_env_var:

Legacy RHTS Task Environment Variables
--------------------------------------

When the `testinfo.desc` file is present, `Restraint` exports the
`RHTS` Legacy variables to support legacy tests written for
`RHTS` (Red Hat Test System).  Both the `testinfo.desc` file
and these variables are being deprecated and the table below lists
the variable substitutes.

+----------------------+----------------------------------+
| RHTS Legacy Variable | Restraint Substitute             |
+======================+==================================+
| ARCH                 | RSTRNT_OSARCH                    |
+----------------------+----------------------------------+
| DISTRO               | RSTRNT_OSDISTRO                  |
+----------------------+----------------------------------+
| FAMILY               | RSTRNT_OSMAJOR                   |
+----------------------+----------------------------------+
| JOBID                | RSTRNT_JOBID                     |
+----------------------+----------------------------------+
| REBOOTCOUNT          | RSTRNT_REBOOTCOUNT               |
+----------------------+----------------------------------+
| RECIPESETID          | RSTRNT_RECIPESETID               |
+----------------------+----------------------------------+
| RECIPEID             | RSTRNT_RECIPEID                  |
+----------------------+----------------------------------+
| RECIPETESTID         | RSTRNT_RECIPEID                  |
+----------------------+----------------------------------+
| RESULT_SERVER        | No equivalent. Communication     |
|                      | only with client/lab controller. |
+----------------------+----------------------------------+
| SUBMITTER            | RSTRNT_OWNER                     |
+----------------------+----------------------------------+
| TASKID               | RSTRNT_TASKID                    |
+----------------------+----------------------------------+
| TESTID               | RSTRNT_TASKID                    |
+----------------------+----------------------------------+
| TESTNAME             | RSTRNT_TASKNAME                  |
+----------------------+----------------------------------+
| TESTPATH             | RSTRNT_TASKPATH                  |
+----------------------+----------------------------------+
| VARIANT              | RSTRNT_OSVARIANT                 |
+----------------------+----------------------------------+

.. [#] `RHTS Task Metadata <https://beaker-project.org/docs/user-guide/task-metadata.html>`_.

