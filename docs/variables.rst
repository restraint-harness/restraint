Variables
=========

The following variables are available to tasks.

* RSTRNT_JOBID - Populated from the job_id attribute of the recipe node.
* RSTRNT_OWNER - Populated from the owner attribute of the job node.
* RSTRNT_RECIPESETID - Populated from the recipe_set_id attribute of the recipe node.
* RSTRNT_RECIPEID - Populated from the id attribute of the recipe node.
* RSTRNT_TASKID - Populated from the id attribute of the task node.
* RSTRNT_OSDISTRO - Name of the distro (only defined if running in beaker)
* RSTRNT_OSMAJOR - Fedora19 or CentOS5
* RSTRNT_OSVARIANT - Server, Client, not all distros use variants
* RSTRNT_OSARCH
* RSTRNT_TASKNAME - Name of task "/distribution/install"
* RSTRNT_TASKPATH - Where the task is installed
* RSTRNT_MAXTIME - Max time in seconds for this task to complete
* RSTRNT_REBOOTCOUNT - The number of times the system has rebooted for this task
* RSTRNT_TASKORDER

These variables are provided in order to support legacy tests written for RHTS

* JOBID - use RSTRNT_JOBID instead
* SUBMITTER - use RSTRNT_OWNER instead
* RECIPESETID - use RSTRNT_RECIPESETID instead
* RECIPEID - use RSTRNT_RECIPEID instead
* RECIPETESTID - use RSTRNT_RECIPEID instead
* TESTID - Use RSTRNT_TASKID instead
* TASKID - use RSTRNT_TASKID instead
* REBOOTCOUNT - use RSTRNT_REBOOTCOUNT instead
* DISTRO - Use RSTRNT_OSDISTRO instead
* VARIANT - Use RSTRNT_OSVARIANT instead
* FAMILY - Use RSTRNT_OSMAJOR instead
* ARCH - Use RSTRNT_OSARCH instead
* TESTNAME - Use RSTRNT_TASKNAME instead
* TESTPATH - Use RSTRNT_TASKPATH instead
* RESULT_SERVER - There is no equivalent, communication is only with the lab controller

