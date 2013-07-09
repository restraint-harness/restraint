#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>
#include <string.h>
#include "recipe.h"
#include "task.h"
#include "metadata.h"
#include "server.h"

#define RESTRAINT_TASK_RUNNER_ERROR restraint_task_runner_error()
GQuark restraint_task_runner_error(void) {
  return g_quark_from_static_string("restraint-task-runner-error-quark");
}

typedef enum {
  RESTRAINT_TASK_RUNNER_ALREADY_RUNNING_ERROR,
} RestraintTaskRunnerError;

SoupSession *soup_session;
SoupRequester *soup_requester;

static void connection_close(ClientConnection *connection)
{
    g_io_stream_close (G_IO_STREAM (connection->connection),NULL, NULL);
    g_source_remove(connection->watch_id);
}

/*
static void connection_watch_close(guint *io_watch_id)
{
    g_source_remove(*io_watch_id);
}
*/

void connections_close(AppData *app_data)
{
    g_list_free_full(app_data->connections, (GDestroyNotify) connection_close);
    app_data->connections = NULL;
}

static void 
connection_write (ClientConnection *connection, GString *s)
{
    gsize bytes_written;
    GOutputStream *ostream = g_io_stream_get_output_stream (G_IO_STREAM (connection->connection));
    g_output_stream_write_all(ostream, s->str, s->len, &bytes_written, NULL, NULL);
}

void connections_write(GList *connections, GString *s)
{
    g_list_foreach (connections, (GFunc) connection_write, s);
}

static gboolean
handle_recipe_request (AppData *app_data, GError **error)
{
  GError *tmp_error = NULL;

  if (app_data->recipe_handler_id == 0) {
    app_data->state = RECIPE_FETCH;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  app_data,
                                                  recipe_finish);
  } else {
    // FIXME close just this connection.
    connections_close(app_data);
    g_set_error(error, RESTRAINT_TASK_RUNNER_ERROR,
                RESTRAINT_TASK_RUNNER_ALREADY_RUNNING_ERROR,
                "Recipe is already running");
    return FALSE;
  }
  return TRUE;

  /* The rest of this will need to move.. */

  if (app_data->recipe == NULL) {
    g_propagate_prefixed_error(error, tmp_error, "Failed to parse recipe: ");
    connections_close(app_data);
    return FALSE;
  }
}

gboolean
network_read(GIOChannel *source,
             GIOCondition cond,
             gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  GString *s = g_string_new(NULL);
  GError *error = NULL;

  if (cond & G_IO_IN) {
    switch (g_io_channel_read_line_string(source, s, NULL, &error)) {
      case G_IO_STATUS_NORMAL:
        app_data->recipe_url = g_strndup(s->str, s->len - 1);
        g_print("Got: %s\n", app_data->recipe_url);
        if (!handle_recipe_request(user_data, &error)) {
          g_warning(error->message);
          g_error_free(error);
          return FALSE;
        }
        return TRUE;
      case G_IO_STATUS_ERROR:
        g_printerr("IO error: %s\n", error->message);
        return FALSE;
      case G_IO_STATUS_EOF:
        g_print("finished!\n");
        return FALSE;
      case G_IO_STATUS_AGAIN:
        g_warning("Not ready.. try again.");
        return TRUE;
      default:
        return FALSE;
        break;
    }
  }
  return FALSE;
}

gboolean new_connection(GSocketService *service,
                           GSocketConnection *connection,
                           GObject *source_object,
                           gpointer user_data)
{
  AppData *app_data = (AppData *) user_data;
  //guint *io_watch_id = g_slice_new0(guint);
  ClientConnection *client_connection = g_slice_new0(ClientConnection);

  // add a reference to connection or it will be closed on us.
  g_object_ref(connection);
  client_connection->connection = connection;

  //GSocketAddress *sockaddr = g_socket_connection_get_remote_address(connection, NULL);
  //GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
  //guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));

  //g_print("New Connection from %s:%d\n", g_inet_address_to_string(addr), port);

  GSocket *socket = g_socket_connection_get_socket(connection);

  gint fd = g_socket_get_fd(socket);
  GIOChannel *channel = g_io_channel_unix_new(fd);
  client_connection->watch_id = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, (GIOFunc) network_read, app_data);
  client_connection->channel = channel;
  app_data->connections = g_list_prepend(app_data->connections, client_connection);
  return TRUE;
}

int main(int argc, char *argv[]) {
  /* initialize glib */
  g_type_init();
  AppData *data = g_slice_new0(AppData);
  gboolean ret;

  data->loop = g_main_loop_new(NULL, FALSE);
  soup_session = soup_session_async_new();
  soup_requester = soup_requester_new();
  soup_session_add_feature(soup_session, SOUP_SESSION_FEATURE(soup_requester));
  soup_session_add_feature_by_type (soup_session, SOUP_TYPE_CONTENT_SNIFFER);

  const gchar *socket_filename = "/tmp/restraint.socket";
  GError *error = NULL;
  GSocketAddress *address = NULL;

  /* create the new socketservice */
  g_unlink(socket_filename);
  GSocketService *service = g_socket_service_new();
  address = g_unix_socket_address_new(socket_filename);
  ret = g_socket_listener_add_address((GSocketListener*) service,
                                       address,
                                       G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL,
                                       NULL,
                                       &error);
  if (!ret) {
    g_error("failed to add address: %s", error->message);
  }

  if (error != NULL) {
    g_error(error->message);
  }

  /* listen to the 'incoming' signal */
  g_signal_connect(service,
                   "incoming",
                   G_CALLBACK(new_connection),
                   data);

  /* start the socket service */
  g_socket_service_start(service);

  /* enter mainloop */
  g_print("Waiting for client!\n");
  g_main_loop_run(data->loop);
  return 0;
}

