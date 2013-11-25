
#ifndef _RESTRAINT_TASK_H
#define _RESTRAINT_TASK_H

#include <glib.h>
#include <libsoup/soup.h>
#include <pty.h>
#include "recipe.h"
#include "server.h"

#define DEFAULT_MAX_TIME 10 * 60 // default amount of time before local watchdog kills process
#define HEARTBEAT 5 * 60 // heartbeat every 5 minutes
#define DEFAULT_ENTRY_POINT "make run"
#define ENV_PREFIX "RSTRNT_"
#define EWD_TIME 5 * 60 // amount of time to add to local watchdog for externl watchdog

typedef enum {
    TASK_IDLE,
    TASK_FETCH,
    TASK_FETCHING,
    TASK_METADATA,
    TASK_ENV,
    TASK_WATCHDOG,
    TASK_DEPENDENCIES,
    TASK_RUN,
    TASK_RUNNING,
    TASK_ABORTED,
    TASK_CANCEL,
    TASK_CANCELLED,
    TASK_FAIL,
    TASK_COMPLETE,
    TASK_COMPLETED,
} TaskSetupState;

typedef enum {
    TASK_FETCH_INSTALL_PACKAGE,
    TASK_FETCH_UNPACK,
} TaskFetchMethod;

#define RESTRAINT_TASK_FETCH_ERROR restraint_task_fetch_error()
GQuark restraint_task_fetch_error(void);
typedef enum {
    RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
    RESTRAINT_TASK_FETCH_ERROR_GIT_NAK,
    RESTRAINT_TASK_FETCH_ERROR_GIT_REMOTE_ERROR,
} RestraintTaskFetchError;

// error domain for libarchive's errnos
#define RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR restraint_task_fetch_libarchive_error()
GQuark restraint_task_fetch_libarchive_error(void);

#define RESTRAINT_TASK_RUNNER_ERROR restraint_task_runner_error()
GQuark restraint_task_runner_error(void);

typedef enum {
  RESTRAINT_TASK_RUNNER_ALREADY_RUNNING_ERROR,
  RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
  RESTRAINT_TASK_RUNNER_STDERR_ERROR,
  RESTRAINT_TASK_RUNNER_FORK_ERROR,
  RESTRAINT_TASK_RUNNER_CHDIR_ERROR,
  RESTRAINT_TASK_RUNNER_EXEC_ERROR,
  RESTRAINT_TASK_RUNNER_RC_ERROR,
  RESTRAINT_TASK_RUNNER_RESULT_ERROR,
} RestraintTaskRunnerError;

typedef struct {
    /* Beaker ID for this task */
    gchar *task_id;
    /* Recipe attributes for this job */
    Recipe *recipe;
    /* Base URI for this task in the Beaker harness API */
    SoupURI *task_uri;
    /* Task name, populated from task metadata if Beaker doesn't tell us */
    gchar *name;
    /* Filesystem path where the task is unpacked */
    gchar *path;
    /* How to fetch this task */
    TaskFetchMethod fetch_method;
    /* Where/what to fetch this task from */
    union {
        gchar *package_name; // when TASK_FETCH_INSTALL_PACKAGE
        SoupURI *url; // when TASK_FETCH_UNPACK
    } fetch;
    /* List of Params */
    GList *params;
    /* List of Roles */
    GList *roles;
    /* Has this task been started already? */
    gboolean started;
    /* Has this task finished already? */
    gboolean finished;
    /* List of dependencies */
    GList *dependencies;
    /* Are we running in rhts_compat mode? */
    gboolean rhts_compat;
    /* entry_point, defaults to make run */
    gchar *entry_point;
    /* maximum time task is allowed to run before being killed */
    guint64 max_time;
    /* maximum time in nicely formatted text */
    gchar expire_time[80];
    /* task order needed for multi-host tasks */
    gint order;
    /* environment variables that will be passed on to task */
    gchar **env;
    /* State engine holding current state of task */
    TaskSetupState state;
    guint pty_handler_id;
    guint pid_handler_id;
    guint timeout_handler_id;
    guint heartbeat_handler_id;
    /* reported result from task */
    gint result;
    /* return code result from exiting task */
    gint pid_result;
    /* pid of running task */
    pid_t pid;
    /* Error at the task level */
    GError *error;
    /* offset of TASKOUT.log */
    gssize offset;
    /* reboot count */
    guint64 reboots;
    /* Filesystem path where the task run data is stored */
    gchar *run_path;
    /* Only true after we have attempted to parse run_metadata on disk. */
    gboolean parsed;
} Task;

Task *restraint_task_new(void);
gboolean task_handler (gpointer user_data);
void task_finish (gpointer user_data);
gboolean restraint_task_fetch_git(AppData *app_data, GError **error);
gboolean restraint_task_fetch_http(AppData *app_data, GError **error);
gboolean restraint_task_fetch(AppData *app_data, GError **error);
gboolean restraint_build_env(Task *task, GError **error);
void restraint_task_status (Task *task, gchar *, GError *reason);
void restraint_task_result (Task *task, gchar *result, guint score, gchar *path, gchar *message);
void restraint_task_run(Task *task);
void restraint_task_free(Task *task);
gboolean idle_task_setup (gpointer user_data);
extern SoupSession *soup_session;
extern char **environ;
int    kill(pid_t, int);
#endif
