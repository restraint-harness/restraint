
#ifndef _RESTRAINT_TASK_H
#define _RESTRAINT_TASK_H

#include <glib.h>
#include <libsoup/soup.h>
#include <pty.h>
#include "recipe.h"

#define DEFAULT_MAX_TIME 10 * 60 // default amount of time before local watchdog kills process
#define DEFAULT_ENTRY_POINT "make run"
#define ENV_PREFIX "RSTRNT_"
#define EWD_TIME 5 * 60 // amount of time to add to local watchdog for externl watchdog

typedef enum {
    TASK_IDLE,
    TASK_FETCH,
    TASK_METADATA,
    TASK_ENV,
    TASK_WATCHDOG,
    TASK_DEPENDENCIES,
    TASK_RUN,
    TASK_RUNNING,
    TASK_ABORTED,
    TASK_CANCELLED,
    TASK_FAIL,
    TASK_COMPLETE,
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
    gchar **entry_point;
    /* maximum time task is allowed to run before being killed */
    guint64 max_time;
    gchar expire_time[80];
    gint order;
    gchar **env;
    TaskSetupState state;
    guint pty_handler_id;
    guint pid_handler_id;
    guint timeout_handler_id;
    guint heartbeat_handler_id;
    gint result;
    gint pid_result;
    pid_t pid;
    GError *error;
} Task;

Task *restraint_task_new(void);
gboolean task_handler (gpointer user_data);
void task_finish (gpointer user_data);
gboolean restraint_task_fetch_git(Task *task, GError **error);
gboolean restraint_task_fetch(Task *task, GError **error);
gboolean restraint_build_env(Task *task, GError **error);
void restraint_task_cancel(Task *task, GError *reason);
void restraint_task_abort(Task *task, GError *reason);
void restraint_task_run(Task *task);
void restraint_task_free(Task *task);
gboolean idle_task_setup (gpointer user_data);
extern SoupSession *soup_session;
extern char **environ;
int    kill(pid_t, int);
#endif
