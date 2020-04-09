.. _env_variables:

Task Environment Variables
==========================

Restraint exports the following environment variables for task use.
They can be altered using the environment variable of the `metadata` file or
`testinfo.desc` file (see :ref:`tasks`).

+----------------------+------------------------------------------------------+-----------+
| Restraint Variables  | Description                                          | Source    |
+======================+======================================================+===========+
| HOME                 | home directory defaults to /root. Can be overwritten | Static    |
|                      | using recipe or task params.                         |           |
+----------------------+------------------------------------------------------+-----------+
| HOSTNAME             | Set by task plugin before execution of user task     | Task      |
|                      |                                                      | Plugin    |
+----------------------+------------------------------------------------------+-----------+
| LANG                 | Environment variable to specify locale.  The default | Static    |
|                      | is `en_US.UTF-8`.  It can be overwritten using       |           |
|                      | recipe or task params.                               |           |
+----------------------+------------------------------------------------------+-----------+
| PATH                 | Program search path environment variable. The default| Static    |
|                      | default is "/usr/local/bin:/usr/bin:/bin:            |           |
|                      | /usr/local/sbin:/usr/sbin:/sbin". It can be          |           |
|                      | overwritten using recipe or task params.             |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_JOBID         | Populated from the job_id attribute of the recipe    | Job       |
|                      | node.                                                |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_MAXTIME       | Max time in seconds for this task to complete.       | Job       |
|                      | Input to local and external watchdog timers.         |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_OSARCH        | OS Architectures. Ex: x86_64, s390x, i386, aarch64,  | Job/Task  |
|                      | ppc64, ppc64le, armhfp                               | Plugin    |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_OSDISTRO      | Name of the distro (Provided if running in Beaker).  | Job       |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_OSMAJOR       | OS Major Version of Distro. Ex: Fedora31, CentOS7,   | Job/Task  |
|                      | RedHatEnterpriseLinux8                               | Plugin    |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_OSVARIANT     | Not all distros use variants. Ex: Server, Client     | Job       |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_OWNER         | Populated from the owner attribute of the job node.  | Job       |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_REBOOTCOUNT   | The number of times the system has rebooted for this | Restraint |
|                      | task. If no reboot occurred, the values is 0.        |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_RECIPEID      | Populated from the id attribute of the recipe node.  | Job       |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_RECIPESETID   | Populated from the recipe_set_id attribute of the    | Job       |
|                      | recipe node.                                         |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_TASKID        | Populated from the id attribute of the task node.    | Job       |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_TASKNAME      | Name of task from the job.                           | metadata  |
|                      | Ex: "/distribution/command".                         |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_TASKORDER     | Sequence Order of tasks multiplied by 2. Used by     | Restraint |
|                      | Restraint when it performs multihosting.             |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_TASKPATH      | Where the task is installed.                         | rpm path/ |
|                      |                                                      | Restraint |
+----------------------+------------------------------------------------------+-----------+
| TERM                 | Terminal type defaults to `vt100`. Can be            | Static    |
|                      | overwritten using recipe or task params.             |           |
+----------------------+------------------------------------------------------+-----------+
| TESTID               | Contains the ID assigned to this task.               | Job       |
+----------------------+------------------------------------------------------+-----------+

For legacy RHTS variables, refer to :ref:`legacy_env_var`.

Script/Plugin Environment Variables
===================================

This table lists environment variables which affect outcome of restraint scripts and plugins.
These variables are often set by the user.  They are as follows:

+----------------------+------------------------------------------------------+-----------+
| Restraint Variables  | Description                                          | Source    |
+======================+======================================================+===========+
| AVC_ERROR            | Refer to :ref:`legacy_rpt_mode` for replacement.     | User      |
+----------------------+------------------------------------------------------+-----------+
| FAILURESTRINGS       | Used by report_result plugin to report user's task.  | User      |
| FALSESTRINGS         | Details can be found :ref:`rpt_result`               |           |
+----------------------+------------------------------------------------------+-----------+
| CLIENTS, SERVERS,    | Assist in the execution of the scripts               | User      |
| DRIVERS              | rstrnt-sync-block/set. :ref:`rstrnt-sync-block`      |           |
+----------------------+------------------------------------------------------+-----------+
| NEXTBOOT_VALID_TIME  | Assist in the execution of the script                | Default/  |
|                      | rstrnt-prepare-reboot. :ref:`p_reboot`               | User      |
+----------------------+------------------------------------------------------+-----------+
| OUTPUTFILE           | Used by localwatchdog plugin to report user's task   | User      |
|                      | output if set.                                       |           |
+----------------------+------------------------------------------------------+-----------+
| TESTPATH/logs2get    | File used by localwatchdog plugin to log user's      | User      |
|                      | files listed in logs2get.                            |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_BACKUP_DIR    | To specify directory when using using Restraint's    | User      |
|                      | backup/restore scripts. :ref:`rstrnt-backup`         |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_DISABLED      | User populated to disable a plugin from running. Do  | User      |
|                      | `RSTRNT_DISABLED="99_reboot"` to prevent `99_reboot` |           |
|                      | from running after local watchdog expires. Do        |           |
|                      | `RSTRNT_DISABLED="01_dmesg_check 10_avc_check"` to   |           |
|                      | prevent multiple error checking plugins from running |           |
|                      | (though disabling these is not advised).             |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_DISABLE_LINGER| Used by task_run plugin to disable user lingering.   | User      |
|                      | Refer to OS command loginctl enable/disable linger   |           |
|                      | for details.  This was introduced due to behavior    |           |
|                      | changes from Fedora24+. Default is to enable.        |           | 
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_LOGGING       | Enables debugging for plugins. Default: 3            | User      |
|                      | (1=Debug, 2=Info, 3=Warning, 4=Error, 5=Critical)    |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_NOPLUGINS     | Set by restraint to disable some plugin functionality| Restraint |
|                      | when "task_run" plugins execute. Further details on  |           |
|                      | this variable can be found :ref:`plugins`.           |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_PKG_CMD       | These variables are used to control the behavior of  | Default/  |
| RSTRNT_PKG_ARGS      | the command rstrnt-package.  For more details, refer | User      |
| RSTRNT_PKG_INSTALL   | to TBD Make reference to rstrnt-package              |           |
| RSTRNT_PKG_REMOVE    |                                                      |           |
| RSTRNT_PKG_RETRIES   |                                                      |           |
| RSTRNT_PKG_DELAY     |                                                      |           |
+----------------------+------------------------------------------------------+-----------+
| RSTRNT_PLUGINS_DIR   | Specifies the directory to run localwatchdog or      | Restraint |
|                      | report_result plugins.                               |           |
+----------------------+------------------------------------------------------+-----------+
