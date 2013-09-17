#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include "recipe.h"
#include "task.h"
#include "metadata.h"
#include "server.h"

SoupSession *soup_session;
SoupServer *soup_server;

static void connection_close(SoupMessage *msg)
{
    g_print ("[%p] Closing client\n", msg);
    soup_message_body_complete (msg->response_body);
    soup_server_unpause_message (soup_server, msg);
}

void connections_close(AppData *app_data)
{
    g_list_free_full(app_data->connections, (GDestroyNotify) connection_close);
    app_data->connections = NULL;
}

static void 
connection_write (SoupMessage *msg, GString *s)
{
    if (s->len) {
        g_print ("[%p] Writing chunk of %lu bytes\n", msg, (unsigned long)s->len);

        soup_message_body_append (msg->response_body,
                                  SOUP_MEMORY_COPY,
                                  s->str,
                                  s->len);
        soup_server_unpause_message (soup_server, msg);
    }
}

void connections_write(GList *connections, GString *s)
{
    write (STDOUT_FILENO, s->str, (size_t)s->len);
    g_list_foreach (connections, (GFunc) connection_write, s);
}

static gboolean
handle_recipe_request (AppData *app_data, GError **error)
{
  if (app_data->state == RECIPE_IDLE) {
    app_data->state = RECIPE_FETCH;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  app_data,
                                                  NULL);
  } else {
    g_set_error(error, RESTRAINT_TASK_RUNNER_ERROR,
                RESTRAINT_TASK_RUNNER_ALREADY_RUNNING_ERROR,
                "Already running a recipe");
    return FALSE;
  }
  return TRUE;
}

static void
server_run_callback (SoupServer *server, SoupMessage *msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  AppData *app_data = (AppData *) data;
  GError *error = NULL;
  gchar *recipe_uri = NULL;
  GHashTable *table;

  // Only accept POST requests for running recipes
  if (msg->method != SOUP_METHOD_POST ) {
    return;
  }

  // decode data from request_body
  table = soup_form_decode(msg->request_body->data);
  recipe_uri = g_hash_table_lookup (table, "recipe");

  // Attempt to run a recipe
  app_data->recipe_url = recipe_uri;
  if (!handle_recipe_request (app_data, &error)) {
    g_warning(error->message);
    soup_message_set_status_full (msg, SOUP_STATUS_BAD_REQUEST, error->message);
    g_error_free (error);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_OK);
  }
}

static void
client_disconnected (SoupMessage *msg, gpointer data)
{
  AppData *app_data = (AppData *) data;

  g_print ("[%p] Client disconnected\n", msg);
  app_data->connections = g_list_remove(app_data->connections, msg);
}

static void
server_monitor_callback (SoupServer *server, SoupMessage *msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  AppData *app_data = (AppData *) data;

  if (app_data->state != RECIPE_IDLE) {
    // Add the current connection to our list of clients watching.
    app_data->connections = g_list_prepend(app_data->connections, msg);

    soup_message_headers_set_encoding (msg->response_headers,
                                     SOUP_ENCODING_CHUNKED);

    // Should we not allow client connection when no recipe is running?
    g_object_ref(msg);
    soup_message_set_status (msg, SOUP_STATUS_OK);

    // Connect the finished signal for this incoming client to client_disconnected
    g_signal_connect (msg, "finished", G_CALLBACK (client_disconnected), app_data);
  } else {
    soup_message_set_status_full (msg, SOUP_STATUS_BAD_REQUEST, "No Recipe Running");
  }
}

static void
server_cancel_callback (SoupServer *server, SoupMessage *msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  AppData *app_data = (AppData *) data;

  if (app_data->tasks) {
    Task *task = (Task *) app_data->tasks->data;

    if (task) {
      task->state = TASK_CANCELLED;
      // Kill the running task
      if (task->pid) {
        kill (task->pid, SIGKILL);
      }
      soup_message_set_status (msg, SOUP_STATUS_OK);
    }
  } else {
      soup_message_set_status_full (msg, SOUP_STATUS_BAD_REQUEST, "No Recipe Running");
  }
  // Kill running recipe and close all connections
  return;
}

int main(int argc, char *argv[]) {
  AppData *data = g_slice_new0(AppData);
  GMainLoop *loop;
  gint port = 8081;

  //soup_session = soup_session_new();
  soup_session = soup_session_new_with_options("idle-timeout", 0, NULL);
  soup_session_add_feature_by_type (soup_session, SOUP_TYPE_CONTENT_SNIFFER);

  soup_server = soup_server_new (SOUP_SERVER_PORT, port, NULL);
  if (!soup_server) {
      g_printerr ("Unable to bind to server port %d\n", port);
      exit (1);
  }

  soup_server_add_handler (soup_server, "/control/run",
                           server_run_callback, data, NULL);
  soup_server_add_handler (soup_server, "/control/monitor",
                           server_monitor_callback, data, NULL);
  soup_server_add_handler (soup_server, "/control/cancel",
                           server_cancel_callback, data, NULL);

  soup_server_run_async (soup_server);

  /* enter mainloop */
  g_print ("Waiting for client!\n");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return 0;
}

