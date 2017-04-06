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
#include "process.h"
#include "message.h"
#include "dependency.h"
#include "config.h"
#include "errors.h"
#include "fetch_git.h"
#include "fetch_http.h"
#include "utils.h"
#include "env.h"

void
restraint_task_result (Task *task, AppData *app_data, gchar *result,
                       gint int_score, gchar *path, gchar *message);

void
archive_entry_callback (const gchar *entry, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GString *message = g_string_new (NULL);

    g_string_printf (message, "** Extracting %s\n", entry);
    connections_write (app_data, LOG_PATH_HARNESS, message->str, message->len);
    g_string_free (message, TRUE);
}

void
taskrun_archive_entry_callback (const gchar *entry, gpointer user_data)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    return archive_entry_callback (entry, task_run_data->app_data);
}

static gboolean fetch_retry (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    Task *task = (Task *) app_data->tasks->data;

    task->state = TASK_FETCH;

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
    return FALSE;
}

void
fetch_finish_callback (GError *error, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    Task *task = (Task *) app_data->tasks->data;

    if (error) {
        if (app_data->fetch_retries < FETCH_RETRIES) {
            g_print("* RETRY [%d]**:%s\n", ++app_data->fetch_retries,
                    error->message);
            g_clear_error(&error);
            g_timeout_add_seconds(FETCH_INTERVAL, fetch_retry, app_data);
            return;
        } else {
            g_propagate_error (&task->error, error);
            task->state = TASK_COMPLETE;
        }
    } else {
        task->state = TASK_METADATA_PARSE;
    }

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

void
restraint_task_fetch(AppData *app_data) {
    g_return_if_fail(app_data != NULL);
    GError *error = NULL;
    Task *task = (Task *) app_data->tasks->data;

    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
        {
            const char *scheme = task->fetch.url->scheme;
            if (g_strcmp0(scheme, "git") == 0) {
                restraint_fetch_git (task->fetch.url,
                                     task->path,
                                     task->keepchanges,
                                     archive_entry_callback,
                                     fetch_finish_callback,
                                     app_data);
            } else if (g_strcmp0(scheme, "http") == 0 ||
                       g_strcmp0(scheme, "https") == 0 ) {
                restraint_fetch_http (task->fetch.url,
                                      task->path,
                                      task->keepchanges,
                                      task->ssl_verify,
                                      archive_entry_callback,
                                      fetch_finish_callback,
                                      app_data);
            } else {
                g_set_error (&error, RESTRAINT_ERROR,
                             RESTRAINT_TASK_RUNNER_SCHEMA_ERROR,
                             "Unimplemented schema method %s",
                             task->fetch.url->scheme);
                fetch_finish_callback (error, app_data);
                return;
            }
            break;
        }
        case TASK_FETCH_INSTALL_PACKAGE:
            ;
            gchar *command = NULL;
            if (task->keepchanges) {
              command = g_strdup_printf ("rstrnt-package install %s",
                                         task->fetch.package_name);
            } else {
              command = g_strdup_printf ("rstrnt-package reinstall %s",
                                         task->fetch.package_name);
            }
            // Use appropriate package install command
            TaskRunData *task_run_data = g_slice_new0(TaskRunData);
            task_run_data->app_data = app_data;
            task_run_data->pass_state = TASK_METADATA_PARSE;
            task_run_data->fail_state = TASK_COMPLETE;
            task_run_data->logpath = LOG_PATH_HARNESS;
            process_run ((const gchar *)command, NULL, NULL, FALSE, 0, task_io_callback, task_handler_callback,
                         app_data->cancellable, task_run_data);
            g_free (command);
            break;
        default:
            // Set task_run_data->error and add task_handler_callback
            g_set_error (&error, RESTRAINT_ERROR,
                         RESTRAINT_TASK_RUNNER_FETCH_ERROR,
                         "Unknown fetch method");
            fetch_finish_callback (error, app_data);
            g_return_if_reached();
    }
}

gboolean
io_callback (GIOChannel *io, GIOCondition condition, const gchar *logpath, gpointer user_data) {
    AppData *app_data = (AppData *) user_data;
    GError *tmp_error = NULL;

    gchar buf[10000];
    gsize bytes_read;

    if (condition & G_IO_IN) {
        //switch (g_io_channel_read_line_string(io, s, NULL, &tmp_error)) {
        switch (g_io_channel_read_chars(io, buf, 10000, &bytes_read, &tmp_error)) {
          case G_IO_STATUS_NORMAL:
            /* Push data to our connections.. */
            if (fwrite(buf, sizeof(gchar), bytes_read, stdout) != bytes_read)
                g_warning ("failed to write message");
            connections_write(app_data, logpath, buf, bytes_read);
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

gboolean
task_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data) {
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    return io_callback(io, condition, task_run_data->logpath, task_run_data->app_data);
}

gboolean
metadata_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data) {
    return io_callback(io, condition, LOG_PATH_HARNESS, user_data);
}

void
task_finish_plugins_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
    g_slice_free(TaskRunData, task_run_data);
}

void
task_finish_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
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

        // If not running in rhts compat mode report PASS for exit code 0
        if (!task->rhts_compat) {
            restraint_task_result(task, app_data, "PASS", 0, "exit_code", NULL);
        }
    } else {
        task->state = task_run_data->fail_state;

        if (localwatchdog) {
            task->localwatchdog = localwatchdog;
            restraint_config_set (app_data->config_file, task->task_id,
                                  "localwatchdog", NULL,
                                  G_TYPE_BOOLEAN, task->localwatchdog);

            g_set_error(&task->error, RESTRAINT_ERROR,
                            RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
                            "Local watchdog expired!");
        } else {
            // If not running in rhts compat mode report FAIL if exit code is not 0
            if (!task->rhts_compat) {
                restraint_task_result(task, app_data, "FAIL", pid_result, 
                                      "exit_code", "Command returned non-zero");
            } else {
                g_set_error(&task->error, RESTRAINT_ERROR,
                            RESTRAINT_TASK_RUNNER_RC_ERROR,
                                "Command returned non-zero %i", pid_result);
            }
        }
    }

    // Run Finish/Completed plugins
    // Always run completed plugins and if localwatchdog triggered run those as well.
    gchar *command = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, PLUGIN_SCRIPT);
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

    task_run_data->logpath = LOG_PATH_HARNESS;
    process_run ((const gchar *) command,
                 (const gchar **) task->env->pdata,
                 "/usr/share/restraint/plugins",
                 FALSE,
                 0,
                 task_io_callback,
                 task_finish_plugins_callback,
                 app_data->cancellable,
                 task_run_data);
    g_free (command);
}

static void
check_param_for_override (Param *param, Task *task)
{
    if (g_strcmp0 (param->name, "KILLTIMEOVERRIDE") == 0 ||
        g_strcmp0 (param->name, "RSTRNT_MAX_TIME") == 0) {
        task->remaining_time = parse_time_string (param->value, NULL);
    }
    if (g_strcmp0 (param->name, "RSTRNT_USE_PTY") == 0) {
        gchar *value = g_ascii_strup(param->value, -1);
        if (g_strcmp0 (value, "TRUE") == 0) {
            task->metadata->use_pty = TRUE;
        } else {
            task->metadata->use_pty = FALSE;
        }
        g_free(value);
    }
}

void metadata_finish_cb(gpointer user_data, GError *error)
{
    AppData *app_data = (AppData *)user_data;
    Task *task = app_data->tasks->data;

    if (error) {
        g_propagate_error(&task->error, error);
    }

    if (task->error || task->metadata == NULL) {
        task->state = TASK_COMPLETE;
    } else {
        task->state = TASK_ENV;

        // Set values from metadata first
        task->remaining_time = task->metadata->max_time;

        // Task param can override task metadata
        g_list_foreach (task->params, (GFunc) check_param_for_override, task);

        // Finally read remaining time from config
        gint64 remaining_time = restraint_config_get_int64 (app_data->config_file,
                                                            task->task_id,
                                                            "remaining_time",
                                                            NULL);

        task->remaining_time = remaining_time == 0 ? task->remaining_time : remaining_time;

        // If task->name is NULL then take name from metadata
        if (task->name == NULL) {
            task->name = task->metadata->name;
        }
    }

    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

void dependency_finish_cb (gpointer user_data, GError *error)
{
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = (AppData *) task_run_data->app_data;
    Task *task = app_data->tasks->data;

    // Remove Heartbeat Handler
    if (task_run_data->heartbeat_handler_id) {
        g_source_remove (task_run_data->heartbeat_handler_id);
        task_run_data->heartbeat_handler_id = 0;
    }

    if (error) {
        g_propagate_error(&task->error, error);
        task->state = TASK_COMPLETE;
    } else {
        task->state = TASK_RUN;
    }
    g_slice_free (TaskRunData, task_run_data);
    app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                task_handler,
                                                app_data,
                                                NULL);
}

void
task_handler_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    /*
     * Generic task handler routine.
     * After running command set task state based on pass/fail of command
     * then re-add the task_handler
     */
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;
    Task *task = app_data->tasks->data;

    if (error) {
        task->error = g_error_copy (error);
        task->state = task_run_data->fail_state;
    } else {
        // Did the command Succeed?
        if (pid_result == 0) {
            task->state = task_run_data->pass_state;
        } else {
            task->state = task_run_data->fail_state;
            g_set_error (&task->error, RESTRAINT_ERROR,
                        RESTRAINT_TASK_RUNNER_RC_ERROR,
                        "Command returned non-zero %i", pid_result);
        }
    }
    // free the task_run_data
    g_slice_free (TaskRunData, task_run_data);

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

    if (!task->metadata->nolocalwatchdog &&
          task_run_data->skip_remaining == FALSE)
        task->remaining_time -= HEARTBEAT;
    restraint_config_set (app_data->config_file, task->task_id,
                          "remaining_time", NULL,
                          G_TYPE_UINT64, task->remaining_time);

    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(currtime,80,"%a %b %d %H:%M:%S %Y", timeinfo);
    g_string_printf(message, "*** Current Time: %s Localwatchdog at: %s\n", currtime, task_run_data->expire_time);
    if (fwrite(message->str, sizeof(gchar), message->len, stderr) != message->len)
        g_warning ("failed to write message");
    connections_write(app_data, LOG_PATH_HARNESS, message->str, message->len);
    g_string_free(message, TRUE);
    return TRUE;
}

void restraint_start_heartbeat(TaskRunData *task_run_data,
                               gint64 remaining_time)
{
    time_t rawtime = time (NULL);

    if (remaining_time != 0) {
        struct tm timeinfo = *localtime (&rawtime);
        timeinfo.tm_sec += remaining_time;
        mktime(&timeinfo);
        strftime(task_run_data->expire_time,
                 sizeof(task_run_data->expire_time),
                 "%a %b %d %H:%M:%S %Y", &timeinfo);
    } else {
        task_run_data->skip_remaining = TRUE;
        snprintf(task_run_data->expire_time,
                  sizeof(task_run_data->expire_time),
                  " * Disabled! *");
    }

    // Local heartbeat, log to console and testout.log
    task_run_data->heartbeat_handler_id = g_timeout_add_seconds_full(
                                               G_PRIORITY_DEFAULT,
                                               HEARTBEAT,
                                               task_heartbeat_callback,
                                               task_run_data,
                                               NULL);
}

void
task_run (AppData *app_data)
{
    Task *task = (Task *) app_data->tasks->data;
    TaskRunData *task_run_data = g_slice_new0 (TaskRunData);
    task_run_data->app_data = app_data;
    task_run_data->pass_state = TASK_COMPLETE;
    task_run_data->fail_state = TASK_COMPLETE;

    gchar *entry_point;
    if (task->metadata->entry_point) {
        entry_point = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, task->metadata->entry_point);
    } else {
        entry_point = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, DEFAULT_ENTRY_POINT);
    }
    if (task->metadata->nolocalwatchdog) {
      task->remaining_time = 0;
    }

    task_run_data->logpath = LOG_PATH_TASK;
    process_run ((const gchar *) entry_point,
                 (const gchar **)task->env->pdata,
                 task->path,
                 task->metadata->use_pty,
                 task->remaining_time,
                 task_io_callback,
                 task_finish_callback,
                 app_data->cancellable,
                 task_run_data);

    g_free (entry_point);

    restraint_start_heartbeat(task_run_data, task->remaining_time);
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
restraint_task_result (Task *task, AppData *app_data, gchar *result,
                       gint int_score, gchar *path, gchar *message)
{
    g_return_if_fail(task != NULL);

    gchar *score = g_strdup_printf("%d", int_score);
    SoupURI *task_results_uri;
    SoupMessage *server_msg;

    task_results_uri = soup_uri_new_with_base(task->task_uri, "results/");
    server_msg = soup_message_new_from_uri("POST", task_results_uri);

    soup_uri_free(task_results_uri);
    g_return_if_fail(server_msg != NULL);

    gchar *data = NULL;
    if (message == NULL) {
        data = soup_form_encode("result", result,
                                "path", path,
                                "score", score, NULL);
    } else {
        data = soup_form_encode("result", result,
                                "path", path,
                                "score", score,
                                "message", message, NULL);
        g_message("%s task %s due to : %s", result, task->task_id, message);
    }
    g_free (score);
    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    app_data->queue_message(soup_session,
                            server_msg,
                            app_data->message_data,
                            NULL,
                            app_data->cancellable,
                            app_data);
}

void
restraint_task_status (Task *task, AppData *app_data, gchar *status, GError *reason)
{
    g_return_if_fail(task != NULL);

    SoupURI *task_status_uri;
    SoupMessage *server_msg;
    gchar *stime = g_strdup_printf("%ld", task->starttime);
    gchar *etime = g_strdup_printf("%ld", task->endtime);

    task_status_uri = soup_uri_new_with_base(task->task_uri, "status");
    server_msg = soup_message_new_from_uri("POST", task_status_uri);

    soup_uri_free(task_status_uri);
    g_return_if_fail(server_msg != NULL);

    gchar *data = NULL;
    if (reason == NULL) {
        data = soup_form_encode("status", status,
                                "stime", stime,
                                "etime", etime,
                                NULL);
    } else {
        data = soup_form_encode("status", status,
                                "stime", stime,
                                "etime", etime,
                                "message", reason->message, NULL);
        g_message("%s task %s due to error: %s", status, task->task_id, reason->message);
    }
    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    g_free(etime);
    g_free(stime);

    app_data->queue_message(soup_session,
                            server_msg,
                            app_data->message_data,
                            task_message_complete,
                            app_data->cancellable,
                            app_data);
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
    seconds_char = g_strdup_printf("%" G_GUINT64_FORMAT, seconds);
    data = soup_form_encode("seconds", seconds_char, NULL);

    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    app_data->queue_message(soup_session,
                            server_msg,
                            app_data->message_data,
                            task_message_complete,
                            app_data->cancellable,
                            app_data);

    g_free(seconds_char);
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    task->remaining_time = -1;
    task->offsets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          g_free);
    return task;
}

void restraint_task_free(Task *task) {
    g_return_if_fail(task != NULL);
    g_free(task->task_id);
    soup_uri_free(task->task_uri);
    g_free(task->name);
    g_free(task->path);
    g_hash_table_destroy(task->offsets);
    switch (task->fetch_method) {
        case TASK_FETCH_INSTALL_PACKAGE:
            g_free(task->fetch.package_name);
            break;
        case TASK_FETCH_UNPACK:
            restraint_free_url(task->fetch.url);
            break;
        default:
            g_return_if_reached();
    }
    g_list_free_full(task->params, (GDestroyNotify) restraint_param_free);
    g_list_free_full(task->roles, (GDestroyNotify) restraint_role_free);
    //g_strfreev (task->env);
    if (task->env)
        g_ptr_array_free (task->env, TRUE);
    restraint_metadata_free(task->metadata);
    g_slice_free(Task, task);
}

gboolean
restraint_next_task (AppData *app_data, TaskSetupState task_state) {
    Task *task = NULL;

    while ((app_data->tasks = g_list_next (app_data->tasks)) != NULL) {
        task = (Task *) app_data->tasks->data;
        task->state = task_state;
        return TRUE;
    }

    // No more tasks, let the recipe_handler know we are done.
    app_data->state = RECIPE_COMPLETE;
    app_data->aborted = ABORTED_NONE;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  app_data,
                                                  recipe_handler_finish);
    return FALSE;
}

gboolean
parse_task_config (gchar *config_file, Task *task, GError **error)
{
    GError *tmp_error = NULL;

    task->reboots = restraint_config_get_uint64 (config_file,
                                                 task->task_id,
                                                 "reboots",
                                                 &tmp_error);
    if (tmp_error) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_task_config,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    gchar *section = g_strdup_printf("offsets_%s", task->task_id);
    gchar **offsets = restraint_config_get_keys(config_file, section, &tmp_error);

    if (tmp_error) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_task_config,", task->task_id);
        g_free(section);
        goto error;
    }
    g_clear_error (&tmp_error);

    if (offsets) {
      gchar **iter = offsets;
      while (*iter) {
        goffset *offset = g_malloc0(sizeof(goffset));
        *offset = restraint_config_get_uint64(config_file, section, *iter,
                                              &tmp_error);
        if (tmp_error) {
            g_propagate_prefixed_error(error, tmp_error,
                        "Task %s:  parse_task_config,", task->task_id);
            g_free(section);
            g_free(offset);
            goto error;
        }
        g_clear_error (&tmp_error);

        g_hash_table_insert(task->offsets, g_strdup(*iter), offset);
        iter++;
      }
      g_strfreev(offsets);
    }
    g_free(section);

    if (tmp_error) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_task_config,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->started = restraint_config_get_boolean (config_file,
                                                  task->task_id,
                                                  "started",
                                                  &tmp_error);
    if (tmp_error) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_task_config,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->localwatchdog = restraint_config_get_boolean (config_file,
                                                        task->task_id,
                                                        "localwatchdog",
                                                        &tmp_error);
    if (tmp_error) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_task_config,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    return TRUE;

error:
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
      if (parse_task_config (app_data->config_file, task, &task->error)) {
          if (task->finished) {
              // If the task is finished skip to the next task.
              task->state = TASK_NEXT;
          } else if (g_cancellable_is_cancelled (app_data->cancellable)) {
              task->state = TASK_COMPLETE;
          } else if (task->localwatchdog) {
              // If the task is not finished but localwatchdog expired.
              g_string_printf(message, "** Localwatchdog task: %s [%s]\n", task->task_id, task->path);
              task->state = TASK_COMPLETE;
          } else if (task->started) {
              // If the task is not finished but started then skip fetching the task again.
              g_string_printf(message, "** Continuing task: %s [%s]\n", task->task_id, task->path);
              task->state = TASK_METADATA_PARSE;
          } else {
              // If neither started nor finished then fetch the task
              restraint_task_status (task, app_data, "Running", NULL);
              result = FALSE;
              g_string_printf(message, "** Fetching task: %s [%s]\n", task->task_id, task->path);
              app_data->fetch_retries = 0;
              task->state = TASK_FETCH;
          }
      } else {
          task->state = TASK_COMPLETE;
      }
      break;
    case TASK_FETCH:
      // Fetch Task from rpm or url
      restraint_task_fetch (app_data);
      result=FALSE;
      break;
    case TASK_METADATA_PARSE:
      g_string_printf (message, "** Preparing metadata\n");
      task->rhts_compat = restraint_get_metadata(task->path,
                            task->recipe->osmajor, &task->metadata,
                            app_data->cancellable, metadata_finish_cb,
                            metadata_io_callback, app_data);
      result=FALSE;
      break;
    case TASK_ENV:
      // Build environment to execute task
      // Includes JOBID, TASKID, OSDISTRO, etc..
      // If not running in rhts_compat mode it will prepend
      // the variables with ENV_PREFIX.
      g_string_printf(message, "** Updating env vars\n");
      build_env(app_data->restraint_url, task);
      task->state = TASK_WATCHDOG;
      break;
    case TASK_WATCHDOG:
      // Setup external watchdog
      if (!task->started) {
          g_string_printf(message, "** Updating external watchdog: %" G_GINT64_FORMAT " seconds\n", task->remaining_time + EWD_TIME);
          restraint_task_watchdog (task, app_data, task->remaining_time + EWD_TIME);
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
          TaskRunData *task_run_data = g_slice_new0(TaskRunData);
          task_run_data->app_data = app_data;
          task_run_data->logpath = LOG_PATH_HARNESS;
          restraint_start_heartbeat(task_run_data, 0);
          restraint_install_dependencies (task, task_io_callback,
                                          taskrun_archive_entry_callback,
                                          dependency_finish_cb,
                                          app_data->cancellable,
                                          task_run_data);
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
      if (g_cancellable_is_cancelled (app_data->cancellable)) {
          task->state = TASK_COMPLETE;
      } else {
          g_string_printf(message, "** Running task: %s [%s]\n", task->task_id, task->name);
          task_run (app_data);
          task->starttime = time(NULL);
          result=FALSE;
          task->started = TRUE;
          restraint_config_set (app_data->config_file,
                                task->task_id,
                                "started", NULL,
                                G_TYPE_BOOLEAN,
                                task->started);
          // Update reboots count
          restraint_config_set (app_data->config_file,
                                task->task_id,
                                "reboots", NULL,
                                G_TYPE_UINT64,
                                task->reboots + 1);
      }
      break;
    case TASK_COMPLETE:
      // Set task finished
      if (g_cancellable_is_cancelled(app_data->cancellable) &&
          app_data->aborted != ABORTED_NONE) {
        g_clear_error(&task->error);
        g_set_error(&task->error, RESTRAINT_ERROR,
                    RESTRAINT_TASK_RUNNER_ABORTED,
                    "Aborted by rstrnt-abort");
      }

      if (task->error) {
          g_string_printf(message, "** ERROR: %s\n** Completed Task : %s\n",
                         task->error->message, task->task_id);
      } else {
          g_string_printf(message, "** Completed Task : %s\n", task->task_id);
      }
      task->endtime = time(NULL);
      task->state = TASK_COMPLETED;
      break;
    case TASK_COMPLETED:
    {
      // Some step along the way failed.
      if (task->error) {
        restraint_task_status(task, app_data, "Aborted", task->error);
        g_clear_error(&task->error);
      } else {
        restraint_task_status(task, app_data, "Completed", NULL);
      }
      // Rmeove the entire [task] section from the config.
      restraint_config_set (app_data->config_file, task->task_id, NULL, NULL, -1);
      task->state = TASK_NEXT;

      if (g_cancellable_is_cancelled(app_data->cancellable) &&
          app_data->aborted == ABORTED_TASK) {
          app_data->aborted = ABORTED_NONE;
        g_cancellable_reset(app_data->cancellable);
      }
      result = FALSE;
      break;
    }
    case TASK_NEXT:
      // Get the next task and run it.
      result = restraint_next_task (app_data, TASK_IDLE);
      break;
    default:
      result = TRUE;
      break;
  }
  if (message->len) {
    if (fwrite(message->str, sizeof(gchar), message->len, stderr) != message->len)
        g_warning ("failed to write message");
    connections_write(app_data, LOG_PATH_HARNESS, message->str, message->len);
  }
  g_string_free(message, TRUE);
  return result;
}
