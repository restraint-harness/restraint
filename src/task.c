
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "task.h"
#include "param.h"
#include "role.h"
#include "metadata.h"
#include "packages.h"
#include "common.h"
#include "process.h"

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
    TaskRunData *task_run_data = (TaskRunData *) user_data;
    AppData *app_data = task_run_data->app_data;
    GError *tmp_error = NULL;

    GString *s = g_string_new(NULL);
    gchar buf[8192];
    gsize bytes_read;

    if (condition & G_IO_IN) {
        //switch (g_io_channel_read_line_string(io, s, NULL, &tmp_error)) {
        switch (g_io_channel_read_chars(io, buf, 8192, &bytes_read, &tmp_error)) {
          case G_IO_STATUS_NORMAL:
            /* Push data to our connections.. */
            g_string_append_len (s, buf, bytes_read);
            connections_write(app_data, s, STREAM_STDOUT, 0);
            g_string_free (s, TRUE);
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
            g_set_error(&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                            RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
                            "Local watchdog expired!");
        } else {
            g_set_error(&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                        RESTRAINT_TASK_RUNNER_RC_ERROR,
                            "Command returned non-zero %i", pid_result);
        }
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
    connections_write(app_data, message, STREAM_STDERR, 0);
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
    const gchar *args[] = {"sh", "-l", "-c", task->entry_point, NULL};
    command_data->command = args;
    command_data->environ = (const gchar **)task->env->pdata;
    command_data->path = task->path;
    command_data->max_time = task->max_time;

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
    struct tm timeinfo = *localtime( &rawtime);
    timeinfo.tm_sec += task->max_time;
    mktime(&timeinfo);
    strftime(task_run_data->expire_time,sizeof(task_run_data->expire_time),"%a %b %d %H:%M:%S %Y", &timeinfo);
    task_run_data->heartbeat_handler_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                           HEARTBEAT, //every 5 minutes
                                                           task_heartbeat_callback,
                                                           task_run_data,
                                                           NULL);
    return TRUE;
}

static gboolean build_env(Task *task, GError **error) {
    //GPtrArray *env = g_ptr_array_new();
    GPtrArray *env = g_ptr_array_new_with_free_func (g_free);
    gchar *prefix = "";
    if (task->rhts_compat == FALSE) {
        prefix = ENV_PREFIX;
        g_ptr_array_add(env, g_strdup_printf("HARNESS_PREFIX=%s", ENV_PREFIX));
    } else {
        g_ptr_array_add(env, g_strdup_printf("DISTRO=%s", task->recipe->osdistro));
        g_ptr_array_add(env, g_strdup_printf("VARIANT=%s", task->recipe->osvariant ));
        g_ptr_array_add(env, g_strdup_printf("FAMILY=%s", task->recipe->osmajor));
        g_ptr_array_add(env, g_strdup_printf("ARCH=%s", task->recipe->osarch));
    }
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
    g_ptr_array_add(env, g_strdup_printf("%sREBOOTCOUNT=%lu", prefix, task->reboots));
    g_ptr_array_add(env, g_strdup_printf("%sLAB_CONTROLLER=", prefix));
    g_ptr_array_add(env, g_strdup_printf("%sTASKORDER=%d", prefix, task->order));
    // HOME, LANG and TERM can be overriden by user by passing it as recipe or task params.
    g_ptr_array_add(env, g_strdup_printf("HOME=/root"));
    g_ptr_array_add(env, g_strdup_printf("TERM=vt100"));
    g_ptr_array_add(env, g_strdup_printf("LANG=en_US.UTF-8"));
    g_ptr_array_add(env, g_strdup_printf("PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"));
//    g_ptr_array_add(env, g_strdup_printf ("%sGUESTS=%s", prefix, task->recipe->guests));
    g_list_foreach(task->recipe->params, (GFunc) build_param_var, env);
    g_list_foreach(task->params, (GFunc) build_param_var, env);
    // Leave three NULL slots for PLUGIN varaibles.
    g_ptr_array_add(env, NULL);
    g_ptr_array_add(env, NULL);
    g_ptr_array_add(env, NULL);
    // This terminates the array
    g_ptr_array_add(env, NULL);
    //task->env = (gchar **) env->pdata;
    task->env = env;
    //g_ptr_array_free (env, FALSE);
    return TRUE;
}

static void
status_message_complete (SoupSession *session, SoupMessage *server_msg, gpointer user_data)
{
    gchar *status = user_data;
    //task_id = user_data;
    if (!SOUP_STATUS_IS_SUCCESSFUL(server_msg->status_code)) {
        g_warning("Updating status to %s Failed for task, libsoup status %u", status, server_msg->status_code);
        // not much else we can do here...
    }
}

static void
watchdog_message_complete (SoupSession *session, SoupMessage *server_msg, gpointer user_data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(server_msg->status_code)) {
        g_warning("Updating watchdog failed for task, libsoup status %u msg: %s %s", server_msg->status_code,
                                                                                  server_msg->reason_phrase,
                                                          soup_uri_to_string(soup_message_get_uri(server_msg), FALSE));
        // depending on the status code we should retry?
        // We should also keep track and report back if we really fail.
    }
}

static void
results_message_complete (SoupSession *session, SoupMessage *server_msg, gpointer user_data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(server_msg->status_code)) {
        g_warning("Reporting result failed for task, libsoup status %u", server_msg->status_code);
        // depending on the status code we should retry?
        // We should also keep track and report back if we really fail.
    }
}

void
restraint_task_status (Task *task, gchar *status, GError *reason)
{
    g_return_if_fail(task != NULL);

    task->status = status;
    restraint_set_run_metadata (task, "status", NULL, G_TYPE_STRING, task->status);

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

    soup_session_queue_message(soup_session, server_msg, status_message_complete, status);
}

void
restraint_task_watchdog (Task *task, guint seconds)
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
    seconds_char = g_strdup_printf("%d", seconds + EWD_TIME);
    data = soup_form_encode("seconds", seconds_char, NULL);

    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    soup_session_queue_message(soup_session, server_msg, watchdog_message_complete, NULL);
}

void
restraint_task_result (Task *task, gchar *result, guint score, gchar *path, gchar *message)
{
    /*
     * result, score, path and message
     */
    g_return_if_fail(task != NULL);
    g_return_if_fail(result != NULL);

    SoupURI *task_results_uri;
    SoupMessage *server_msg;

    task_results_uri = soup_uri_new_with_base(task->task_uri, "results/");
    server_msg = soup_message_new_from_uri("POST", task_results_uri);

    soup_uri_free(task_results_uri);
    g_return_if_fail(server_msg != NULL);

    gchar *data = NULL;
    GHashTable *data_table = NULL;
    data_table = g_hash_table_new (NULL, NULL);
    g_hash_table_insert (data_table, "result", result);
    if (score)
        g_hash_table_insert (data_table, "score", &score);
    if (path)
        g_hash_table_insert (data_table, "path", path);
    if (message)
        g_hash_table_insert (data_table, "message", message);
    data = soup_form_encode_hash (data_table);

    soup_message_set_request(server_msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    soup_session_queue_message(soup_session, server_msg, results_message_complete, NULL);
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    task->max_time = 0;
    task->result_id = 0;
    task->entry_point = g_strdup_printf ("%s", DEFAULT_ENTRY_POINT);
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
    g_free (task->entry_point);
    //g_strfreev (task->env);
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
                                                  recipe_finish);
    return FALSE;
}

gboolean
task_handler (gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;

  // No More tasks, return false so this handler gets removed.
  if (app_data->tasks == NULL)
      return FALSE;

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
          task->state = TASK_COMPLETED;
      } else if (task->started) {
          // If the task is not finished but started then skip fetching the task again.
          g_string_printf(message, "** Continuing task: %s [%s]\n", task->task_id, task->path);
          task->state = TASK_METADATA;
      } else {
          // If neither started nor finished then fetch the task
          g_string_printf(message, "** Fetching task: %s [%s]\n", task->task_id, task->path);
          task->state = TASK_FETCH;
      }
      break;
    case TASK_FETCH:
      // Fetch Task from rpm or url
      if (restraint_task_fetch (app_data, &task->error))
          task->state = TASK_FETCHING;
      else
          task->state = TASK_COMPLETE;
      break;
    case TASK_FETCHING:
        return FALSE;
    case TASK_METADATA:
      // If running in rhts_compat mode and testinfo.desc doesn't exist
      // then we need to generate it.
      if (restraint_is_rhts_compat (task)) {
          task->rhts_compat = TRUE;
          if (restraint_no_testinfo (task)) {
              if (restraint_generate_testinfo (app_data, &task->error)) {
                  task->state = TASK_METADATA_GEN;
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
    case TASK_METADATA_GEN:
        return FALSE;
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
      if (build_env(task, &task->error))
        task->state = TASK_WATCHDOG;
      else
        task->state = TASK_COMPLETE;
      break;
    case TASK_WATCHDOG:
      // Setup external watchdog
      if (!task->started) {
          g_string_printf(message, "** Updating watchdog\n");
          // FIXME If KILLTIMEOVERRIDE is defined use that instead of max_time.
          restraint_task_watchdog (task, task->max_time);
      }
      task->state = TASK_DEPENDENCIES;
      break;
    case TASK_DEPENDENCIES:
      // Install Task Dependencies
      // All dependencies are installed with system package command
      // All repodependencies are installed via fetch_git
      if (!task->started) {
          g_string_printf(message, "** Installing dependencies\n");
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
        restraint_task_status (task, "Running", NULL);
        task->state = TASK_RUNNING;
        task->started = TRUE;
        restraint_set_run_metadata (task, "started", NULL, G_TYPE_BOOLEAN, task->started);
        // Update reboots count
        restraint_set_run_metadata (task, "reboots", NULL, G_TYPE_UINT64, task->reboots + 1);
      } else {
        task->state = TASK_COMPLETE;
      }
      break;
    case TASK_RUNNING:
        // TASK is running.
        // Remove this idle handler
        // The task_pid_complete will re-add this handler with the 
        //  state TASK_COMPLETE
        return FALSE;
    case TASK_COMPLETE:
      // Some step along the way failed.
      if (task->error) {
        g_warning("%s\n", task->error->message);
        g_string_printf(message, "** ERROR: %s\n", task->error->message);
        restraint_task_status(task, "Aborted", task->error);
      } else {
        restraint_task_status(task, "Completed", NULL);
      }
      // Set task finished
      task->finished = TRUE;
      restraint_set_run_metadata (task, "finished", NULL, G_TYPE_BOOLEAN, task->finished);

      g_string_printf(message, "** Completed Task : %s\n", task->task_id);
      task->state = TASK_COMPLETED;
      break;
    case TASK_COMPLETED:
      // Get the next task and run it.
      result = restraint_next_task (app_data, TASK_IDLE);
      break;
    default:
      result = TRUE;
      break;
  }
  if (message->len) {
    connections_write(app_data, message, STREAM_STDERR, 0);
    g_string_free(message, TRUE);
  }
  return result;
}

void
restraint_init_result_hash (AppData *app_data)
{
    static gint none = 0;
    static gint pass = 1;
    static gint warn = 2;
    static gint fail = 3;

    GHashTable *hash_to = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash_to, "NONE", &none);
    g_hash_table_insert(hash_to, "PASS", &pass);
    g_hash_table_insert(hash_to, "WARN", &warn);
    g_hash_table_insert(hash_to, "FAIL", &fail);

    GHashTable *hash_from = g_hash_table_new(g_int_hash, g_int_equal);
    g_hash_table_insert(hash_from, &none, "NONE");
    g_hash_table_insert(hash_from, &pass, "PASS");
    g_hash_table_insert(hash_from, &warn, "WARN");
    g_hash_table_insert(hash_from, &fail, "FAIL");

    app_data->result_states_to = hash_to;
    app_data->result_states_from = hash_from;
}
