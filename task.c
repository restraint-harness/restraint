
#include <string.h>
#include <glib.h>

#include "task.h"
#include "param.h"
#include "role.h"
#include "metadata.h"
#include "packages.h"
#include "server.h"

GQuark restraint_task_fetch_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-error-quark");
}

GQuark restraint_task_fetch_libarchive_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-libarchive-error-quark");
}

gboolean restraint_task_fetch(Task *task, GError **error) {
    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *tmp_error = NULL;
    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
            if (g_strcmp0(soup_uri_get_scheme(task->fetch.url), "git") == 0) {
                if (!restraint_task_fetch_git(task, &tmp_error)) {
                    g_propagate_error(error, tmp_error);
                    return FALSE;
                }
            } else {
                g_critical("XXX IMPLEMENTME");
                return FALSE;
            }
            break;
        case TASK_FETCH_INSTALL_PACKAGE:
            if (!restraint_install_package(task->fetch.package_name, &tmp_error)) {
                g_propagate_error(error, tmp_error);
                return FALSE;
            }
            break;
        default:
            g_return_val_if_reached(FALSE);
    }
    return TRUE;
}

static void build_param_var(Param *param, GPtrArray *env) {
    g_ptr_array_add(env, g_strdup_printf("%s=%s", param->name, param->value));
}

static gboolean build_env(Task *task, GError **error) {
    GPtrArray *env = g_ptr_array_new();
    gchar *prefix = "";
    if (task->rhts_compat == FALSE)
        prefix = ENV_PREFIX;
    g_list_foreach(task->recipe->roles, (GFunc) build_param_var, env);
    g_list_foreach(task->roles, (GFunc) build_param_var, env);
    g_ptr_array_add(env, g_strdup_printf("%sJOBID=%s", prefix, task->recipe->job_id));
    g_ptr_array_add(env, g_strdup_printf("%sRECIPESETID=%s", prefix, task->recipe->recipe_set_id));
    g_ptr_array_add(env, g_strdup_printf("%sRECIPEID=%s", prefix, task->recipe->recipe_id));
    g_ptr_array_add(env, g_strdup_printf("%sTASKID=%s", prefix, task->task_id));
    g_ptr_array_add(env, g_strdup_printf("%sOSDISTRO=%s", prefix, task->recipe->osdistro));
    g_ptr_array_add(env, g_strdup_printf("%sOSMAJOR=%s", prefix, task->recipe->osmajor));
    g_ptr_array_add(env, g_strdup_printf("%sOSVARIANT=%s", prefix, task->recipe->osvariant ));
    g_ptr_array_add(env, g_strdup_printf("%sOSARCH=%s", prefix, task->recipe->osarch));
    g_ptr_array_add(env, g_strdup_printf("%sTASKPATH=%s", prefix, task->path));
    g_ptr_array_add(env, g_strdup_printf("%sTASKNAME=%s", prefix, task->name));
    g_ptr_array_add(env, g_strdup_printf("%sMAXTIME=%lu", prefix, task->max_time));
    g_ptr_array_add(env, g_strdup_printf("%sLAB_CONTROLLER=", prefix));
    g_ptr_array_add(env, g_strdup_printf("%sTASKORDER=%d", prefix, task->order));
    // HOME and LANG can be overriden by user by passing it as recipe or task params.
    g_ptr_array_add(env, g_strdup_printf("HOME=/root"));
    g_ptr_array_add(env, g_strdup_printf("LANG=en_US.UTF-8"));
//    g_ptr_array_add(env, g_strdup_printf ("%sGUESTS=%s", prefix, task->recipe->guests));
    g_list_foreach(task->recipe->params, (GFunc) build_param_var, env);
    g_list_foreach(task->params, (GFunc) build_param_var, env);
    task->env = (gchar **) env->pdata;
    g_ptr_array_free (env, FALSE);
    return TRUE;
}

static void
abort_message_complete (SoupSession *session, SoupMessage *msg, void *dummy)
{
    //task_id = user_data;
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
        g_warning("Failed to abort task, libsoup status %u", msg->status_code);
        // not much else we can do here...
    }
}

void restraint_task_abort(Task *task, GError *reason) {
    g_return_if_fail(task != NULL);

    SoupURI *task_status_uri;
    SoupMessage *msg;

    task_status_uri = soup_uri_new_with_base(task->task_uri, "status");
    msg = soup_message_new_from_uri("POST", task_status_uri);

    soup_uri_free(task_status_uri);
    g_return_if_fail(msg != NULL);

    gchar *data = NULL;
    if (reason == NULL) {
        // this is basically a bug, but to be nice let's handle it
        g_warning("Aborting task with no reason given");
        data = soup_form_encode("status", "Aborted", NULL);
    } else {
        data = soup_form_encode("status", "Aborted",
                "message", reason->message, NULL);
        g_message("Aborting task %s due to error: %s", task->task_id, reason->message);
    }
    soup_message_set_request(msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    soup_session_queue_message(soup_session, msg, abort_message_complete, NULL);
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    task->max_time = DEFAULT_MAX_TIME;
    task->entry_point = g_strdup(DEFAULT_ENTRY_POINT);
    return task;
}

void restraint_task_free(Task *task) {
    g_return_if_fail(task != NULL);
    g_free(task->task_id);
    soup_uri_free(task->task_uri);
    g_free(task->name);
    g_free(task->path);
    switch (task->fetch_method) {
        case TASK_FETCH_INSTALL_PACKAGE:
            g_free(task->fetch.package_name);
            break;
        case TASK_FETCH_UNPACK:
            soup_uri_free(task->fetch.url);
            break;
        default:
            g_return_if_reached();
    }
    g_list_free_full(task->params, (GDestroyNotify) restraint_param_free);
    g_list_free_full(task->roles, (GDestroyNotify) restraint_role_free);
    g_free(task->entry_point);
    g_list_free_full(task->dependencies, (GDestroyNotify) g_free);
    g_slice_free(Task, task);
}

void
task_finish (gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  Task *task = NULL;

  // Iterate to the next task
  app_data->tasks = g_list_next (app_data->tasks);
  if (app_data->tasks) {
    task = (Task *) app_data->tasks->data;
    // Yes, there is a task. add another task_handler.
    task->state = TASK_FETCH;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    task_handler, 
                    app_data,
                    task_finish);
  } else {
    // No more tasks, let the recipe_handler know we are done.
    app_data->task_handler_id = 0;
  }
  return;
}

gboolean
task_handler (gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  Task *task = app_data->tasks->data;
  GString *msg = g_string_new(NULL);
  gboolean result = TRUE;

  /*
   *  - Fecth the task
   *  - Update metadata
   *  - Build env variables
   *  - Update external Watchdog
   *  - Add localwatchdog timeout
   *  - Install dependencies
   *  - Run task
   *  - Add child pid watcher
   *  - Add io_add_watch on pty output
   */
  switch (task->state) {
    case TASK_FETCH:
      // Fetch Task from rpm or url
      // Only git:// is supported currently.
      g_string_printf(msg, "Fetching task: %s\n", task->task_id);
      if (restraint_task_fetch(task, &task->error))
        task->state = TASK_METADATA;
      else
        task->state = TASK_FAIL;
      break;
    case TASK_METADATA:
      // Update Task metadata
      // entry_point, defaults to "make run"
      // max_time which is used by both localwatchdog and externalwatchdog
      // dependencies required for task execution
      // rhts_compat is set to false if new "metadata" file exists.
      g_string_printf(msg, "Updating metadata for task: %s\n", task->task_id);
      if (restraint_metadata_update(task, &task->error))
        task->state = TASK_ENV;
      else
        task->state = TASK_FAIL;
      break;
    case TASK_ENV:
      // Build environment to execute task
      // Includes JOBID, TASKID, OSDISTRO, etc..
      // If not running in rhts_compat mode it will prepend
      // the variables with ENV_PREFIX.
      g_string_printf(msg, "Updating env vars for task: %s\n", task->task_id);
      if (build_env(task, &task->error)) // FIXME update env arg.
        task->state = TASK_WATCHDOG;
      else
        task->state = TASK_FAIL;
      break;
    case TASK_WATCHDOG:
      // Setup external watchdog
      // Add EWD_TIME to task->max_time and use that for the external watchdog.
      g_string_printf(msg, "Updating watchdog for task: %s\n", task->task_id);
      task->state = TASK_DEPENDENCIES;
      break;
    case TASK_DEPENDENCIES:
      // Install Task Dependencies
      // All dependencies are installed with system package command
      // All repodependencies are installed via fetch_git
      g_string_printf(msg, "Installing dependencies for task: %s\n", task->task_id);
      task->state = TASK_RUN;
      break;
    case TASK_RUN:
      // Run TASK
      // Setup pid_handler
      //       pty_handler
      //       timeout_handler
      //       heartbeat_handler
      g_string_printf(msg, "Running task: %s\n", task->task_id);
      result = FALSE;
      break;
    case TASK_RUNNING:
      // Check if task->pid_handler_id is still active
      //          task->pyt_handler_id
    case TASK_FAIL:
      // Some step along the way failed.
      if (task->error) {
        g_warning("%s\n", task->error->message);
        g_string_printf(msg, "ERROR: %s\n", task->error->message);
        restraint_task_abort(task, task->error);
        g_error_free(task->error);
      }
      result = FALSE;
      break;
    default:
      // We should never get here!
      break;
  }
  connections_write(app_data->connections, msg);
  g_string_free(msg, TRUE);
  return result;
}
