/*  
    This file is part of Restraint.

    Restraint is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Restraint is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Restraint.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _RESTRAINT_TASK_H
#define _RESTRAINT_TASK_H

#include <glib.h>
#include <libsoup/soup.h>
#include <pty.h>
#include "recipe.h"
#include "message.h"
#include "server.h"
#include "metadata.h"
#include "utils.h"

#define DEFAULT_MAX_TIME 10 * 60 // default amount of time before local watchdog kills process
#define HEARTBEAT 1 * 60 // heartbeat every 1 minute
#define DEFAULT_ENTRY_POINT "make run"
#define ENV_PREFIX "RSTRNT_"
#define EWD_TIME 30 * 60 // amount of time to add to local watchdog for externl watchdog

#define LOG_PATH_HARNESS "logs/harness.log"
#define LOG_PATH_TASK "logs/taskout.log"

typedef enum {
    TASK_IDLE,
    TASK_FETCH,
    TASK_METADATA_PARSE,
    TASK_ENV,
    TASK_WATCHDOG,
    TASK_DEPENDENCIES,
    TASK_RUN,
    TASK_ABORTED,
    TASK_CANCEL,
    TASK_CANCELLED,
    TASK_NEXT,
    TASK_COMPLETE,
    TASK_COMPLETED,
} TaskSetupState;

typedef enum {
    TASK_FETCH_INSTALL_PACKAGE,
    TASK_FETCH_UNPACK,
} TaskFetchMethod;

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
        struct restraint_url *url; // when TASK_FETCH_UNPACK
    } fetch;
    /* Whether to keep task changes */
    gboolean keepchanges;
    gboolean ssl_verify;
    /* List of Params */
    GList *params;
    /* List of Roles */
    GList *roles;
    /* Has this task been started already? */
    gboolean started;
    /* Has this task finished already? */
    gboolean finished;
    /* Has this task triggered the localwatchdog? */
    gboolean localwatchdog;
    /* Are we running in rhts_compat mode? */
    gboolean rhts_compat;
    /* remaining time task is allowed to run before being killed */
    gint64 remaining_time;
    /* task order needed for multi-host tasks */
    gint order;
    /* environment variables that will be passed on to task */
    GPtrArray *env;
    /* State engine holding current state of task */
    TaskSetupState state;
    /* Error at the task level */
    GError *error;
    /* Log file offsets */
    GHashTable *offsets;
    /* reboot count */
    guint64 reboots;
    MetaData *metadata;
} Task;

typedef struct {
    AppData *app_data;
    TaskSetupState pass_state;
    TaskSetupState fail_state;
    gint heartbeat_handler_id;
    gchar expire_time[80];
    const gchar *logpath;
} TaskRunData;

Task *restraint_task_new(void);
gboolean task_handler (gpointer user_data);
void task_finish (gpointer user_data);
gboolean restraint_task_fetch_http(AppData *app_data, GError **error);
void
restraint_task_fetch(AppData *app_data);
gboolean restraint_build_env(Task *task, GError **error);
void restraint_task_status (Task *task, AppData *app_data, gchar *, GError *reason);
void restraint_task_run(Task *task);
void restraint_task_free(Task *task);
void restraint_init_result_hash (AppData *app_data);
gboolean task_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data);
void task_handler_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error);
gboolean idle_task_setup (gpointer user_data);
extern SoupSession *soup_session;
#endif
