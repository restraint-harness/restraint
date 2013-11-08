#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include "recipe.h"
#include "task.h"
#include "metadata.h"
#include "common.h"
#include "server.h"

#define MAX_RETRIES 5
SoupSession *soup_session;
SoupServer *soup_server;

static void
copy_header (const char *name, const char *value, gpointer dest_headers)
{
        soup_message_headers_append (dest_headers, name, value);
}

static void
retry_message (SoupSession *session, SoupMessage *failed_msg, SoupSessionCallback callback, gint *retries)
{
    SoupMessage *retry_msg;
    retry_msg = soup_message_new_from_uri (failed_msg->method, soup_message_get_uri (failed_msg));
    soup_message_headers_foreach (failed_msg->request_headers, copy_header,
                                  retry_msg->request_headers);
    SoupBuffer *request = soup_message_body_flatten (failed_msg->request_body);
    soup_message_body_append_buffer (retry_msg->request_body, request);
    soup_buffer_free (request);
    soup_session_queue_message (soup_session, retry_msg, callback, retries);
}

static void
connection_close (SoupMessage *client_msg)
{
    g_print ("[%p] Closing client\n", client_msg);
    soup_message_body_complete (client_msg->response_body);
    soup_server_unpause_message (soup_server, client_msg);
}

static gboolean
is_not_monitor (SoupMessage *client_msg)
{
    GHashTable *table;
    table = soup_form_decode(client_msg->request_body->data);

    if (g_hash_table_lookup_extended (table, "monitor", NULL, NULL))
        return FALSE;
    else
        return TRUE;
}

/**
 * If monitor is TRUE close all connections
 * otherwise only close connections that didn't request monitoring
 */
void connections_close (AppData *app_data, gboolean monitor)
{
    GList *connections = app_data->connections;
    SoupMessage *client_msg = NULL;

    // Remove all clients
    if (monitor) {
      g_list_foreach (app_data->connections, (GFunc) connection_close, app_data->error);
      g_list_free(app_data->connections);
      app_data->connections = NULL;
    // Remove any client not requesting monitoring
    } else if (connections) {
      do {
        client_msg = connections->data;
        if (is_not_monitor (client_msg)) {
          app_data->connections = g_list_remove(app_data->connections, client_msg);
          connection_close(client_msg);
        }
      } while ((connections = g_list_next (connections)) != NULL);
    }
}

static void 
connection_write (SoupMessage *client_msg, GString *s)
{
    //g_printerr ("[%p] Writing chunk of %lu bytes\n", client_msg, (unsigned long)s->len);

    soup_message_body_append (client_msg->response_body,
                              SOUP_MEMORY_COPY,
                              s->str,
                              s->len);
    soup_server_unpause_message (soup_server, client_msg);
}

static void
task_output_complete (SoupSession *session, SoupMessage *server_msg, gpointer user_data)
{
    gint *retries = (gint *) user_data;

    if (SOUP_STATUS_IS_SUCCESSFUL(server_msg->status_code)) {
        // Update run_metadata file with new taskout.log offset.
        restraint_set_run_metadata (task, "offset", NULL, G_TYPE_UINT64, task->offset);
        goto finish;
    }

    if (server_msg->status_code >= 500) {
        if (*retries < MAX_RETRIES) {
            *retries = *retries + 1;
            g_warning("Retrying %d of %d, taskoutput.log code:%u Reason:%s\n",
                      *retries, MAX_RETRIES, server_msg->status_code, server_msg->reason_phrase);
            retry_message (soup_session, server_msg, task_output_complete, retries);
            return;
        } else {
            g_warning("Max retries %d, Failed to send taskoutput.log\n", MAX_RETRIES);
        }
    } else {
        g_warning("Failed to send taskoutput.log code:%u Reason:%s\n",
                  server_msg->status_code, server_msg->reason_phrase);
    }
finish:
    g_free (retries);
}

static void
task_output (Task *task, GString *message)
{
    g_return_if_fail (task != NULL);
    g_return_if_fail (message != NULL);

    // Initalize our retry variable.
    gint *retries = malloc( sizeof(*retries) );
    *retries = 0;

    SoupURI *task_output_uri;
    SoupMessage *server_msg;

    task_output_uri = soup_uri_new_with_base (task->task_uri, "logs/taskoutput.log");
    server_msg = soup_message_new_from_uri ("PUT", task_output_uri);
    soup_uri_free (task_output_uri);
    g_return_if_fail (server_msg != NULL);

    gchar *range = g_strdup_printf ("bytes %zu-%zu/*", task->offset, task->offset + message->len - 1);
    task->offset = task->offset + message->len;
    soup_message_headers_append (server_msg->request_headers, "Content-Range", range);
    soup_message_set_request (server_msg, "text/plain", SOUP_MEMORY_COPY, message->str, message->len);
    soup_session_queue_message (soup_session, server_msg, task_output_complete, retries);
}

void connections_write (AppData *app_data, GString *message, gint stream_type, gint status)
{
    GString *meta = g_string_new(NULL);

    write (STDOUT_FILENO, message->str, (size_t)message->len);

    // Setup mixed content header
    g_string_printf (meta,
                     "Content-Type: text/plain\n"
                     "Content-ID: %d\n"
                     "Status: %d\n"
                     "Content-Length: %d\r\n\r\n",
                     stream_type, status, (guint)message->len);
    meta = g_string_append (meta, message->str);
    meta = g_string_append(meta, "\r\n--cut-here");
    g_list_foreach (app_data->connections, (GFunc) connection_write, meta);
    g_string_free (meta, TRUE);

    // Active parsed task?  Send the output to taskout.log via REST
    if (app_data->tasks && ((Task *) app_data->tasks->data)->parsed)
        task_output ((Task *) app_data->tasks->data, message);
}

/**
 * Send an error code to all clients.  You should only send this once so that
 * means you should close the connection after this.
 **/
void
connections_error (AppData *app_data, GError *error)
{
    g_return_if_fail (app_data != NULL);
    g_return_if_fail (error != NULL);

    GString *s = g_string_new(error->message);
    connections_write (app_data, s, STREAM_ERROR, error->code);
    g_string_free(s, TRUE);
}

static gboolean
handle_recipe_request (AppData *app_data, GError **error)
{
  if (app_data->state == RECIPE_IDLE) {
    app_data->state = RECIPE_FETCH;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  app_data,
                                                  recipe_finish);
  } else {
    g_set_error(error, RESTRAINT_TASK_RUNNER_ERROR,
                RESTRAINT_TASK_RUNNER_ALREADY_RUNNING_ERROR,
                "Already running a recipe");
    return FALSE;
  }
  return TRUE;
}

static void
client_disconnected (SoupMessage *client_msg, gpointer data)
{
  AppData *app_data = (AppData *) data;

  g_print ("[%p] Client disconnected\n", client_msg);
  app_data->connections = g_list_remove(app_data->connections, client_msg);
}

static void
server_control_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  AppData *app_data = (AppData *) data;
  GError *error = NULL;
  gchar *recipe_url = NULL;
  GHashTable *table;
  gboolean monitor = FALSE;
  gboolean cancel = FALSE;
  GString *boundary = g_string_new("--cut-here");

  // Only accept POST requests for running recipes
  if (client_msg->method != SOUP_METHOD_POST ) {
    return;
  }

  // decode data from request_body
  table = soup_form_decode(client_msg->request_body->data);
  recipe_url = g_hash_table_lookup (table, "recipe");
  monitor = g_hash_table_lookup_extended (table, "monitor", NULL, NULL);
  cancel = g_hash_table_lookup_extended (table, "cancel", NULL, NULL);

  // Attempt to run a recipe if requested.
  if (recipe_url) {
    app_data->recipe_url = recipe_url;
    if (!handle_recipe_request (app_data, &error)) {
      g_warning(error->message);
      soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, error->message);
      g_error_free (error);
      return;
    }
  } else if (monitor && app_data->state == RECIPE_IDLE) {
      soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, "No Recipe Running");
      return;
  }

  // Add the current connection to our list of clients watching.
  app_data->connections = g_list_prepend(app_data->connections, client_msg);
  connection_write(client_msg, boundary);

  soup_message_headers_set_encoding (client_msg->response_headers,
                                     SOUP_ENCODING_EOF);
  soup_message_headers_append (client_msg->response_headers,
                               "Content-Type", "multipart/x-mixed-replace; boundary=--cut-here");
  g_object_ref(client_msg);

  // Connect the finished signal for this incoming client to client_disconnected
  g_signal_connect (client_msg, "finished", G_CALLBACK (client_disconnected), app_data);
  soup_message_set_status (client_msg, SOUP_STATUS_OK);

  // Cancel the running recipe if requested.
  if (app_data->tasks && cancel) {
    Task *task = (Task *) app_data->tasks->data;
    task->state = TASK_CANCELLED;
    // Kill the running task #FIXME check that we actually killed it
    if (task->pid) {
      kill (task->pid, SIGKILL);
    }
  }
}

/**
 * Abort the remaining tasks
 **/
static void
recipe_abort_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
}

/**
 * Report task result
 **/
static void
task_result_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
}

/**
 * Update local watchdog and push external watchdog out as well.
 **/
static void
task_watchdog_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
}

/**
 * Abort current task
 **/
static void
task_abort_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
}

/**
 * Split on /tasks/(task_id)
 * /recipes/(recipe_id)/tasks/(task_id)/results/(result_id)/logs/(path: path)
 **/
static SoupURI*
parse_url (SoupMessage *client_msg, Task *task)
{
  gchar *uristr;
  gchar *tasks_id;
  gchar **server_path;
  SoupURI *soup_uri;

  uristr = soup_uri_to_string (soup_message_get_uri (client_msg), FALSE);
  tasks_id = g_strconcat("/tasks/", task->task_id, "/", NULL);
  server_path = g_strsplit (uristr, tasks_id, 2);
  g_free (uristr);
  soup_uri = soup_uri_new_with_base (task->task_uri, server_path[1]);
  g_strfreev(server_path);

  return soup_uri;
}

/**
 * Proxy the uploading log files to the lab controller.
 *
 * We could also transfer these to any restraint client that happens to be 
 * monitoring the recipe.
 **/
static void
log_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  SoupMessage *server_msg;
  SoupURI *result_log_uri;
  AppData *app_data = (AppData *) data;

  if (app_data->state != RECIPE_IDLE && app_data->tasks) {
    Task *task = (Task *) app_data->tasks->data;
    result_log_uri = parse_url(client_msg, task);
    server_msg = soup_message_new_from_uri ("POST", result_log_uri);
    soup_message_headers_foreach (client_msg->request_headers, copy_header,
                                  server_msg->request_headers);
    if (client_msg->request_body->length) {
      SoupBuffer *request = soup_message_body_flatten (client_msg->request_body);
      soup_message_body_append_buffer (server_msg->request_body, request);
      soup_buffer_free (request);
    }
    soup_session_send_message (soup_session, server_msg);
    soup_message_set_status (client_msg, server_msg->status_code);
  }
}

void
server_register_recipe_callbacks (AppData *app_data)
{
  // POST /recipes/(recipe_id)/watchdog
  // POST /recipes/(recipe_id)/status
  // PUT /recipes/(recipe_id)/logs/(path: path)
  //Task *task = (Task *) app_data->tasks->data;

  //task_status = g_strdup_printf ("/recipes/%s/tasks/%s/status");
  // Recipe handlers
  soup_server_add_handler (soup_server, "/recipe/abort",
                           recipe_abort_callback, app_data, NULL);
}

void
server_register_task_callbacks (AppData *app_data)
{
  // POST /recipes/(recipe_id)/tasks/(task_id)/status
  // POST /recipes/(recipe_id)/tasks/(task_id)/results/
  // PUT /recipes/(recipe_id)/logs/(path: path)
  // PUT /recipes/(recipe_id)/tasks/(task_id)/logs/(path: path)
  // PUT /recipes/(recipe_id)/tasks/(task_id)/results/(result_id)/logs/(path: path)
  //
  //Task *task = (Task *) app_data->tasks->data;

  //task_status = g_strdup_printf ("/recipes/%s/tasks/%s/status",
  //                               task->recipe_id, task->task_id);
  //task_result = g_strdup_printf ("/recipes/%s/tasks/%s/results/",
  //                               task->recipe_id, task->task_id);
  //task_log = g_strdup_printf ("/recipes/%s/tasks/%s/logs",
  //                            task->recipe_id, task->task_id);
  //result_log = g_strdup_printf ("/recipes/%s/tasks/%s/results/%s/logs",
  //                              task->recipe_id, task->task_id, ##);

  // Task handlers
  soup_server_add_handler (soup_server, "/task/results",
                           task_result_callback, app_data, NULL);
  soup_server_add_handler (soup_server, "/task/watchdog",
                           task_watchdog_callback, app_data, NULL);
  soup_server_add_handler (soup_server, "/task/abort",
                           task_abort_callback, app_data, NULL);
  soup_server_add_handler (soup_server, "/logs/task/",
                           log_callback, app_data, NULL);
}

int main(int argc, char *argv[]) {
  AppData *data = g_slice_new0(AppData);
  GMainLoop *loop;
  gint port = 8081;

  soup_session = soup_session_new_with_options("idle-timeout", 0, NULL);
  soup_session_add_feature_by_type (soup_session, SOUP_TYPE_CONTENT_SNIFFER);

  soup_server = soup_server_new (SOUP_SERVER_PORT, port, NULL);
  if (!soup_server) {
      g_printerr ("Unable to bind to server port %d\n", port);
      exit (1);
  }

  // Handler for controlling the harness
  soup_server_add_handler (soup_server, "/control",
                           server_control_callback, data, NULL);
// Fallback handler.. 
//  soup_server_add_handler (soup_server, NULL,
//                           server_idle, data, NULL);



  soup_server_run_async (soup_server);

  /* enter mainloop */
  g_print ("Waiting for client!\n");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return 0;
}

