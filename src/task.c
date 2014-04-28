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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "task.h"
#include "param.h"
#include "role.h"
#include "metadata.h"
#include "packages.h"
#include "process.h"
#include "message.h"
#include "dependency.h"

GQuark restraint_task_runner_error(void) {
    return g_quark_from_static_string("restraint-task-runner-error-quark");
}

GQuark restraint_task_fetch_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-error-quark");
}

GQuark restraint_task_fetch_libarchive_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-libarchive-error-quark");
}

gboolean restraint_task_fetch(AppData *app_data, GError **error) {
    g_return_val_if_fail(app_data != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    Task *task = (Task *) app_data->tasks->data;

    GError *tmp_error = NULL;
    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
            if (g_strcmp0(soup_uri_get_scheme(task->fetch.url), "git") == 0) {
                if (!restraint_task_fetch_git(app_data, &tmp_error)) {
                    g_propagate_error(error, tmp_error);
                    return FALSE;
                }
            } else if (g_strcmp0(soup_uri_get_scheme(task->fetch.url), "http") == 0) {
                if (!restraint_task_fetch_http(app_data, &tmp_error)) {
                    g_propagate_error(error, tmp_error);
                    return FALSE;
                }
            } else {
                g_critical("XXX IMPLEMENTME");
                return FALSE;
            }
            break;
        case TASK_FETCH_INSTALL_PACKAGE:
            if (!restraint_install_package(app_data, task->fetch.package_name, &tmp_error)) {
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

gboolean
task_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data) {
    ProcessData *process_data = (ProcessData *) user_data;
    TaskRunData *task_run_data = process_data->user_data;
    AppData *app_data = task_run_data->app_data;
    GError *tmp_error = NULL;

    gchar buf[131072];
    gsize bytes_read;

    if (condition & G_IO_IN) {
        //switch (g_io_channel_read_line_string(io, s, NULL, &tmp_error)) {
        switch (g_io_channel_read_chars(io, buf, 131072, &bytes_read, &tmp_error)) {
          case G_IO_STATUS_NORMAL:
            /* Push data to our connections.. */
            write (STDOUT_FILENO, buf, bytes_read);
            connections_write(app_data, buf, bytes_read);
            return TRUE;

          case G_IO_STATUS_ERROR:
             g_printerr("IO error: %s\n", tmp_error->message);
             g_clear_error (&tmp_error);
             return FALSE;

          case G_IO_STATUS_EOF:
             g_print("finished!\n");
             return FALSE;

          case G_IO_STATUS_AGAIN:
             g_warning("Not ready.. try again.");
             return TRUE;

          default:
             g_return_val_if_reached(FALSE);
             break;
        }
    }
    if (condition & G_IO_HUP){
        return FALSE;
    }

    return FALSE;
}

void
task_finish_plugins_callback (gint pid_result, gboolean localwatchdog, gpointer user_data)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

void
task_finish_callback (gint pid_result, gboolean localwatchdog, gpointer user_data)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;
    Task *task = app_data->tasks->data;

    // Remove Heartbeat Handler
    if (task_run_data->heartbeat_handler_id) {
        g_source_remove (task_run_data->heartbeat_handler_id);
        task_run_data->heartbeat_handler_id = 0;
    }

    // Did the command Succeed?
    if (pid_result == 0) {
        task->state = task_run_data->pass_state;
    } else {
        task->state = task_run_data->fail_state;

        if (localwatchdog) {
            task->localwatchdog = localwatchdog;
            restraint_set_run_metadata (task, "localwatchdog", NULL, G_TYPE_BOOLEAN, task->localwatchdog);

            g_set_error(&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                            RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
                            "Local watchdog expired!");
        } else {
            g_set_error(&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                        RESTRAINT_TASK_RUNNER_RC_ERROR,
                            "Command returned non-zero %i", pid_result);
        }
    }

    // Run Finish/Completed plugins
    // Always run completed plugins and if localwatchdog triggered run those as well.
    CommandData *command_data = g_slice_new0 (CommandData);
    const gchar *command[] = {TASK_PLUGIN_SCRIPT, PLUGIN_SCRIPT, NULL};
    command_data->command = command;
    // Last four entries are NULL.  Replace first three with plugin vars
    gchar *localwatchdog_plugin = g_strdup_printf(" %s/localwatchdog.d", PLUGIN_DIR);
    gchar *plugin_dir = g_strdup_printf("RSTRNT_PLUGINS_DIR=%s/completed.d%s", PLUGIN_DIR, localwatchdog ? localwatchdog_plugin : "");
    g_free (localwatchdog_plugin);
    if (task->env->pdata[task->env->len - 5] != NULL) {
        g_free (task->env->pdata[task->env->len - 5]);
    }
    task->env->pdata[task->env->len - 5] = plugin_dir;

    gchar *no_plugins = g_strdup_printf("RSTRNT_NOPLUGINS=1");
    if (task->env->pdata[task->env->len - 4] != NULL) {
        g_free (task->env->pdata[task->env->len - 4]);
    }
    task->env->pdata[task->env->len - 4] = no_plugins;

    gchar *rstrnt_localwatchdog = g_strdup_printf("RSTRNT_LOCALWATCHDOG=%s", localwatchdog ? "TRUE" : "FALSE");
    if (task->env->pdata[task->env->len - 3] != NULL) {
        g_free (task->env->pdata[task->env->len - 3]);
    }
    task->env->pdata[task->env->len - 3] = rstrnt_localwatchdog;

    command_data->environ = (const gchar **) task->env->pdata;
    command_data->path = "/usr/share/restraint/plugins";

    GError *tmp_error = NULL;
    if (process_run (command_data,
                     task_io_callback,
                     task_finish_plugins_callback,
                     task_run_data,
                     &tmp_error)) {
    } else {
        g_warning ("run_plugins failed to run: %s\n", tmp_error->message);
        g_clear_error (&tmp_error);
    }
}

void
task_handler_callback (gint pid_result, gboolean localwatchdog, gpointer user_data)
{
    /*
     * Generic task handler routine.
     * After running command set task state based on pass/fail of command
     * then re-add the task_handler
     */
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;
    Task *task = app_data->tasks->data;

    // Did the command Succeed?
    if (pid_result == 0) {
        task->state = task_run_data->pass_state;
    } else {
        task->state = task_run_data->fail_state;
        g_set_error (&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                    RESTRAINT_TASK_RUNNER_RC_ERROR,
                    "Command returned non-zero %i", pid_result);
    }

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

gboolean
task_heartbeat_callback (gpointer user_data)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;
    Task *task = (Task *) app_data->tasks->data;

    time_t rawtime;
    struct tm * timeinfo;
    GString *message = g_string_new(NULL);
    gchar currtime[80];

    task->max_time -= HEARTBEAT;
    restraint_set_run_metadata (task, "max_time", NULL, G_TYPE_UINT64, task->max_time);

    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(currtime,80,"%a %b %d %H:%M:%S %Y", timeinfo);
    g_string_printf(message, "*** Current Time: %s Localwatchdog at: %s\n", currtime, task_run_data->expire_time);
    write (STDERR_FILENO, message->str, message->len);
    connections_write(app_data, message->str, message->len);
    g_string_free(message, TRUE);
    return TRUE;
}

static gboolean
task_run (AppData *app_data, GError **error)
{
    Task *task = (Task *) app_data->tasks->data;
    time_t rawtime = time (NULL);
    TaskRunData *task_run_data = g_slice_new0 (TaskRunData);
    task_run_data->app_data = app_data;
    task_run_data->pass_state = TASK_COMPLETE;
    task_run_data->fail_state = TASK_COMPLETE;

    CommandData *command_data = g_slice_new0 (CommandData);
    command_data->command = (const gchar **) task->entry_point;

    command_data->environ = (const gchar **)task->env->pdata;
    command_data->path = task->path;
    if (!task->nolocalwatchdog) {
        command_data->max_time = task->max_time;
        struct tm timeinfo = *localtime (&rawtime);
        timeinfo.tm_sec += task->max_time;
        mktime(&timeinfo);
        strftime(task_run_data->expire_time,
                 sizeof(task_run_data->expire_time),
                 "%a %b %d %H:%M:%S %Y", &timeinfo);
    } else {
        snprintf (task_run_data->expire_time,
                  sizeof(task_run_data->expire_time),
                  " * Disabled! *");
    }

    GError *tmp_error = NULL;

    if (!process_run (command_data,
                      task_io_callback,
                      task_finish_callback,
                      task_run_data,
                      &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error,
                                    "Task %s failed to run", task->task_id);
        return FALSE;
    }

    // Local heartbeat, log to console and testout.log
    task_run_data->heartbeat_handler_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                           HEARTBEAT, //every 5 minutes
                                                           task_heartbeat_callback,
                                                           task_run_data,
                                                           NULL);
    return TRUE;
}

static void
array_add (GPtrArray *array, const gchar *prefix, const gchar *variable, const gchar *value)
{
    if (value) {
        if (prefix) {
            g_ptr_array_add (array, g_strdup_printf ("%s%s=%s", prefix, variable, value));
        } else {
            g_ptr_array_add (array, g_strdup_printf ("%s=%s", variable, value));
        }
    }
}

static void build_env(Task *task) {
    //GPtrArray *env = g_ptr_array_new();
    GPtrArray *env = g_ptr_array_new_with_free_func (g_free);

    g_list_foreach(task->recipe->roles, (GFunc) build_param_var, env);
    g_list_foreach(task->roles, (GFunc) build_param_var, env);

    gchar *prefix = ENV_PREFIX;
    if (task->rhts_compat == TRUE) {
        array_add (env, NULL, "RESULT_SERVER", "LEGACY");
        array_add (env, NULL, "SUBMITTER", task->recipe->owner);
        array_add (env, NULL, "JOBID", task->recipe->job_id);
        array_add (env, NULL, "RECIPESETID", task->recipe->recipe_set_id);
        array_add (env, NULL, "RECIPEID", task->recipe->recipe_id);
        array_add (env, NULL, "RECIPETESTID", task->task_id);
        array_add (env, NULL, "TASKID", task->task_id);
        array_add (env, NULL, "DISTRO", task->recipe->osdistro);
        array_add (env, NULL, "VARIANT", task->recipe->osvariant);
        array_add (env, NULL, "FAMILY", task->recipe->osmajor);
        array_add (env, NULL, "ARCH", task->recipe->osarch);
        array_add (env, NULL, "TESTNAME", task->name);
        array_add (env, NULL, "TESTPATH", task->path);
        array_add (env, NULL, "TESTID", task->task_id);
        g_ptr_array_add(env, g_strdup_printf("MAXTIME=%" PRIu64, task->max_time));
        g_ptr_array_add(env, g_strdup_printf("REBOOTCOUNT=%" PRIu64, task->reboots));
        g_ptr_array_add(env, g_strdup_printf("TASKORDER=%d", task->order));
    }
    g_ptr_array_add(env, g_strdup_printf("HARNESS_PREFIX=%s", ENV_PREFIX));
    array_add (env, prefix, "RECIPE_URL", soup_uri_to_string (task->recipe->recipe_uri, FALSE));
    array_add (env, prefix, "OWNER", task->recipe->owner);
    array_add (env, prefix, "JOBID", task->recipe->job_id);
    array_add (env, prefix, "RECIPESETID", task->recipe->recipe_set_id);
    array_add (env, prefix, "RECIPEID", task->recipe->recipe_id);
    array_add (env, prefix, "TASKID", task->task_id);
    array_add (env, prefix, "OSDISTRO", task->recipe->osdistro);
    array_add (env, prefix, "OSMAJOR", task->recipe->osmajor);
    array_add (env, prefix, "OSVARIANT", task->recipe->osvariant);
    array_add (env, prefix, "OSARCH", task->recipe->osarch);
    array_add (env, prefix, "TASKNAME", task->name);
    array_add (env, prefix, "TASKPATH", task->path);
    g_ptr_array_add(env, g_strdup_printf("%sMAXTIME=%" PRIu64, prefix, task->max_time));
    g_ptr_array_add(env, g_strdup_printf("%sREBOOTCOUNT=%" PRIu64, prefix, task->reboots));
    //g_ptr_array_add(env, g_strdup_printf("%sLAB_CONTROLLER=", prefix));
    g_ptr_array_add(env, g_strdup_printf("%sTASKORDER=%d", prefix, task->order));
    // HOME, LANG and TERM can be overriden by user by passing it as recipe or task params.
    g_ptr_array_add(env, g_strdup_printf("HOME=/root"));
    g_ptr_array_add(env, g_strdup_printf("TERM=vt100"));
    g_ptr_array_add(env, g_strdup_printf("LANG=en_US.UTF-8"));
    g_ptr_array_add(env, g_strdup_printf("PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"));

    // Override with recipe level params
    g_list_foreach(task->recipe->params, (GFunc) build_param_var, env);
    // Override with task level params
    g_list_foreach(task->params, (GFunc) build_param_var, env);
    // Leave four NULL slots for PLUGIN varaibles.
    g_ptr_array_add(env, NULL);
    g_ptr_array_add(env, NULL);
    g_ptr_array_add(env, NULL);
    g_ptr_array_add(env, NULL);
    // This terminates the array
    g_ptr_array_add(env, NULL);
    task->env = env;
}

static void
task_message_complete (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
    // Add the task_handler back to the main loop
    AppData *app_data = (AppData *) user_data;
    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

void
restraint_task_status (Task *task, AppData *app_data, gchar *status, GError *reason)
{
    g_return_if_fail(task != NULL);

    SoupURI *task_status_uri;
    SoupMessage *server_msg;

    task_status_uri = soup_uri_new_with_base(task->task_uri, "status");
    server_msg = soup_message_new_from_uri("POST", task_status_uri);

    soup_uri_free(task_status_uri);
    g_return_if_fail(server_msg != NULL);

    gchar *data = NULL;
    if (reason == NULL) {
        data = soup_form_encode("status", status, NULL);
    } else {
        data = soup_form_encode("status", status,
                "message", reason->message, NULL);
        g_message("%s task %s due to error: %s", status, task->task_id, reason->message);
    }
    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    restraint_queue_message(soup_session, server_msg, task_message_complete, app_data);
}

void
restraint_task_watchdog (Task *task, AppData *app_data, guint64 seconds)
{
    g_return_if_fail(task != NULL);
    g_return_if_fail(seconds != 0);

    gchar *seconds_char = NULL;
    SoupURI *recipe_watchdog_uri;
    SoupMessage *server_msg;
    gchar *data = NULL;

    recipe_watchdog_uri = soup_uri_new_with_base(task->recipe->recipe_uri, "watchdog");
    server_msg = soup_message_new_from_uri("POST", recipe_watchdog_uri);

    soup_uri_free(recipe_watchdog_uri);
    g_return_if_fail(server_msg != NULL);

    // Add EWD_TIME to seconds and use that for the external watchdog.
    seconds_char = g_strdup_printf("%" PRIu64, seconds);
    data = soup_form_encode("seconds", seconds_char, NULL);

    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    restraint_queue_message(soup_session, server_msg, task_message_complete, app_data);
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    task->max_time = 0;
    gchar *entry_point = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, DEFAULT_ENTRY_POINT);
    task->entry_point = g_strsplit (entry_point, " ", 0);
    g_free (entry_point);
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
    g_strfreev (task->entry_point);
    //g_strfreev (task->env);
    if (task->env)
        g_ptr_array_free (task->env, TRUE);
    g_list_free_full(task->dependencies, (GDestroyNotify) g_free);
    g_slice_free(Task, task);
}

gboolean
restraint_next_task (AppData *app_data, TaskSetupState task_state) {
    Task *task = app_data->tasks->data;

    while ((app_data->tasks = g_list_next (app_data->tasks)) != NULL) {
        task = (Task *) app_data->tasks->data;
        task->state = task_state;
        return TRUE;
    }

    // No more tasks, let the recipe_handler know we are done.
    app_data->state = RECIPE_COMPLETE;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  app_data,
                                                  NULL);
    return FALSE;
}

gboolean
task_handler (gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;

  // No More tasks, return false so this handler gets removed.
  if (app_data->tasks == NULL) {
      g_print ("no more tasks..\n");
      return FALSE;
  }

  Task *task = app_data->tasks->data;
  GString *message = g_string_new(NULL);
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
    case TASK_IDLE:
      // Read in previous state..
      restraint_parse_run_metadata (task, NULL);

      if (task->finished) {
          // If the task is finished skip to the next task.
          task->state = TASK_NEXT;
      } else if (task->localwatchdog) {
          // If the task is not finished but localwatchdog expired.
          g_string_printf(message, "** Localwatchdog task: %s [%s]\n", task->task_id, task->path);
          task->state = TASK_COMPLETE;
      } else if (task->started) {
          // If the task is not finished but started then skip fetching the task again.
          g_string_printf(message, "** Continuing task: %s [%s]\n", task->task_id, task->path);
          task->state = TASK_METADATA;
      } else {
          // If neither started nor finished then fetch the task
          restraint_task_status (task, app_data, "Running", NULL);
          result = FALSE;
          g_string_printf(message, "** Fetching task: %s [%s]\n", task->task_id, task->path);
          task->state = TASK_FETCH;
      }
      break;
    case TASK_FETCH:
      // Fetch Task from rpm or url
      if (restraint_task_fetch (app_data, &task->error))
          result=FALSE;
      else
          task->state = TASK_COMPLETE;
      break;
    case TASK_METADATA:
      // If running in rhts_compat mode and testinfo.desc doesn't exist
      // then we need to generate it.
      if (restraint_is_rhts_compat (task)) {
          task->rhts_compat = TRUE;
          if (restraint_no_testinfo (task)) {
              if (restraint_generate_testinfo (app_data, &task->error)) {
                  result=FALSE;
                  g_string_printf(message, "** Generating testinfo.desc\n");
              } else {
                  task->state = TASK_COMPLETE;
              }
          } else {
              task->state = TASK_METADATA_PARSE;
          }
      } else {
          // not running in rhts_compat mode, skip to next step where we parse the config.
          task->rhts_compat = FALSE;
          task->state = TASK_METADATA_PARSE;
      }
      break;
    case TASK_METADATA_PARSE:
      // Update Task metadata
      // entry_point, defaults to "make run"
      // max_time which is used by both localwatchdog and externalwatchdog
      // dependencies required for task execution
      // rhts_compat is set to false if new "metadata" file exists.
      g_string_printf(message, "** Parsing metadata\n");
      if (restraint_metadata_parse(task, &task->error)) {
        task->state = TASK_ENV;
        restraint_set_run_metadata (task, "name", NULL, G_TYPE_STRING, task->name);
      } else {
        task->state = TASK_COMPLETE;
      }
      break;
    case TASK_ENV:
      // Build environment to execute task
      // Includes JOBID, TASKID, OSDISTRO, etc..
      // If not running in rhts_compat mode it will prepend
      // the variables with ENV_PREFIX.
      g_string_printf(message, "** Updating env vars\n");
      build_env(task);
      task->state = TASK_WATCHDOG;
      break;
    case TASK_WATCHDOG:
      // Setup external watchdog
      if (!task->started) {
          g_string_printf(message, "** Updating external watchdog: %" PRIu64 " seconds\n", task->max_time + EWD_TIME);
          restraint_task_watchdog (task, app_data, task->max_time + EWD_TIME);
          result=FALSE;
      }
      task->state = TASK_DEPENDENCIES;
      break;
    case TASK_DEPENDENCIES:
      // Install Task Dependencies
      // All dependencies are installed with system package command
      // All repodependencies are installed via fetch_git
      if (!task->started) {
          g_string_printf(message, "** Installing dependencies\n");
          restraint_install_dependencies (app_data);
          result=FALSE;
      }
      task->state = TASK_RUN;
      break;
    case TASK_RUN:
      // Run TASK
      // Setup pid_handler
      //       io_handler
      //       timeout_handler
      //       heartbeat_handler
      g_string_printf(message, "** Running task: %s [%s]\n", task->task_id, task->name);
      if (task_run (app_data, &task->error)) {
        result=FALSE;
        task->started = TRUE;
        restraint_set_run_metadata (task, "started", NULL, G_TYPE_BOOLEAN, task->started);
        // Update reboots count
        restraint_set_run_metadata (task, "reboots", NULL, G_TYPE_UINT64, task->reboots + 1);
      } else {
        task->state = TASK_COMPLETE;
      }
      break;
    case TASK_COMPLETE:
      // Set task finished
      task->finished = TRUE;
      restraint_set_run_metadata (task, "finished", NULL, G_TYPE_BOOLEAN, task->finished);

      if (task->error) {
          g_string_printf(message, "** ERROR: %s\n** Completed Task : %s\n",
                         task->error->message, task->task_id);
      } else {
          g_string_printf(message, "** Completed Task : %s\n", task->task_id);
      }
      task->state = TASK_COMPLETED;
      break;
    case TASK_COMPLETED:
      // Some step along the way failed.
      if (task->error) {
        restraint_task_status(task, app_data, "Aborted", task->error);
      } else {
        restraint_task_status(task, app_data, "Completed", NULL);
      }
      task->state = TASK_NEXT;
      result = FALSE;
      break;
    case TASK_NEXT:
      // Get the next task and run it.
      result = restraint_next_task (app_data, TASK_IDLE);
      break;
    default:
      result = TRUE;
      break;
  }
  if (message->len) {
    write (STDERR_FILENO, message->str, message->len);
    connections_write(app_data, message->str, message->len);
    g_string_free(message, TRUE);
  }
  return result;
}
