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

#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "recipe.h"
#include "task.h"
#include "errors.h"
#include "config.h"
#include "server.h"
#include "process.h"
#include "message.h"

#define MAX_RETRIES 5
SoupSession *soup_session;
GMainLoop *loop;

static void
copy_header (const char *name, const char *value, gpointer dest_headers)
{
        soup_message_headers_append (dest_headers, name, value);
}

/**
 * Take orig and split on split_string and put new_base on beginning.
 **/
static gchar *
swap_base (const gchar *orig, const gchar *new_base, gchar *split_string)
{
  gchar *new;
  gchar **orig_pieces;
  gchar **new_pieces;

  orig_pieces = g_strsplit (orig, split_string, 2);
  new_pieces = g_strsplit (new_base, split_string, 2);
  new = g_strconcat(new_pieces[0], split_string, orig_pieces[1], NULL);
  g_strfreev(orig_pieces);
  g_strfreev(new_pieces);

  return new;
}

static void restraint_free_app_data(AppData *app_data)
{
  g_free(app_data->recipe_url);
  g_free(app_data->config_file);
  g_free(app_data->restraint_url);

  if (app_data->recipe != NULL) {
    restraint_recipe_free(app_data->recipe);
    app_data->recipe = NULL;
  }

  g_clear_error(&app_data->error);
  g_slice_free(AppData, app_data);
}

void connections_write (AppData *app_data, gchar *msg_data, gsize msg_len)
{
    // Active parsed task?  Send the output to taskout.log via REST
    if (app_data->tasks) {
        Task *task = (Task *) app_data->tasks->data;
        SoupURI *task_output_uri = soup_uri_new_with_base (task->task_uri, "logs/taskoutput.log");
        SoupMessage *server_msg = soup_message_new_from_uri ("PUT", task_output_uri);
        soup_uri_free (task_output_uri);
        g_return_if_fail (server_msg != NULL);

        gchar *range = g_strdup_printf ("bytes %zu-%zu/*", task->offset, task->offset + msg_len - 1);
        task->offset = task->offset + msg_len;
        soup_message_headers_append (server_msg->request_headers, "Content-Range", range);
        g_free (range);

        soup_message_headers_append (server_msg->request_headers, "log-level", "2");
        soup_message_set_request (server_msg, "text/plain", SOUP_MEMORY_COPY, msg_data, msg_len);

        restraint_queue_message (soup_session, server_msg, NULL, NULL);

        // Update config file with new taskout.log offset.
        restraint_config_set (app_data->config_file, task->task_id,
                              "offset", NULL,
                              G_TYPE_UINT64, task->offset);
    }
}



gboolean
server_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data) {
    //ProcessData *process_data = (ProcessData *) user_data;
    ServerData *server_data = (ServerData *) user_data;
    //ServerData *server_data = process_data->user_data;
    AppData *app_data = server_data->app_data;
    GError *tmp_error = NULL;

    gchar buf[131072];
    gsize bytes_read;

    if (condition & G_IO_IN) {
        //switch (g_io_channel_read_line_string(io, s, NULL, &tmp_error)) {
        switch (g_io_channel_read_chars(io, buf, 131072, &bytes_read, &tmp_error)) {
          case G_IO_STATUS_NORMAL:
            write (STDOUT_FILENO, buf, bytes_read);
            /* Push data to our connections.. */
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

static gboolean
handle_recipe_request (gchar *recipe_url, ServerData *server_data, GError **error)
{
  AppData *app_data = server_data->app_data;

  if (app_data->state == RECIPE_IDLE) {
    app_data->recipe_url = recipe_url;
    app_data->state = RECIPE_FETCH;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  server_data,
                                                  recipe_handler_finish);
  } else {
    g_set_error(error, RESTRAINT_ERROR,
                RESTRAINT_ALREADY_RUNNING_RECIPE_ERROR,
                "Already running a recipe");
    return FALSE;
  }
  return TRUE;
}

void
plugin_finish_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    ServerData *server_data = (ServerData *) user_data;
    if (error) {
        g_warning ("** ERROR: running plugins, %s\n", error->message);
    }
    if (pid_result != 0) {
        g_warning ("** ERROR: running plugins returned non-zero %i\n", pid_result);
    }
    soup_server_unpause_message (server_data->server, server_data->client_msg);
}

static void
server_msg_complete (SoupSession *session, SoupMessage *server_msg, gpointer user_data)
{
    ServerData *server_data = (ServerData *) user_data;
    AppData *app_data = server_data->app_data;
    SoupMessage *client_msg = server_data->client_msg;
    Task *task = app_data->tasks->data;
    GHashTable *table;
    gboolean no_plugins = FALSE;

    //SOUP_STATUS_IS_SUCCESSFUL(server_msg->status_code

    soup_message_headers_foreach (server_msg->response_headers, copy_header,
                                  client_msg->response_headers);
    if (server_msg->response_body->length) {
      SoupBuffer *request = soup_message_body_flatten (server_msg->response_body);
      soup_message_body_append_buffer (client_msg->response_body, request);
      soup_buffer_free (request);
    }
    soup_message_set_status (client_msg, server_msg->status_code);

    if (g_str_has_suffix (server_data->path, "/results/")) {
        // Very important that we don't run plugins from results
        // reported from the plugins themselves.
        table = soup_form_decode (client_msg->request_body->data);
        no_plugins = g_hash_table_lookup_extended (table, "no_plugins", NULL, NULL);

        // Execute report plugins
        if (!no_plugins) {
            // Create a new ProcessCommand
            gchar *command = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, PLUGIN_SCRIPT);

            // Last four entries are NULL.  Replace first three with plugin vars
            gchar *result_server = g_strdup_printf("RSTRNT_RESULT_URL=%s", soup_message_headers_get_one (client_msg->response_headers, "Location"));
            if (task->env->pdata[task->env->len - 5] != NULL) {
                g_free (task->env->pdata[task->env->len - 5]);
            }
            task->env->pdata[task->env->len - 5] = result_server;

            gchar *plugin_dir = g_strdup_printf("RSTRNT_PLUGINS_DIR=%s/report_result.d", PLUGIN_DIR);
            if (task->env->pdata[task->env->len - 4] != NULL) {
                g_free (task->env->pdata[task->env->len - 4]);
            }
            task->env->pdata[task->env->len - 4] = plugin_dir;

            gchar *no_plugins = g_strdup_printf("RSTRNT_NOPLUGINS=1");
            if (task->env->pdata[task->env->len - 3] != NULL) {
                g_free (task->env->pdata[task->env->len - 3]);
            }
            task->env->pdata[task->env->len - 3] = no_plugins;
            gchar *disable_plugin = g_hash_table_lookup (table, "disable_plugin");
            if (disable_plugin) {
                gchar *disabled_plugins = g_strdup_printf ("RSTRNT_DISABLED=%s", disable_plugin);
                if (task->env->pdata[task->env->len - 2] != NULL) {
                    g_free (task->env->pdata[task->env->len - 2]);
                }
                task->env->pdata[task->env->len - 2] = disabled_plugins;
            }

            process_run ((const gchar *) command,
                         (const gchar **) task->env->pdata,
                         "/usr/share/restraint/plugins",
                         0,
                         server_io_callback,
                         plugin_finish_callback,
                         server_data);
            g_free (command);
        }
        g_hash_table_destroy (table);
    } else {
        soup_server_unpause_message (server_data->server, client_msg);
        g_slice_free (ServerData, server_data);
    }

    // If no plugins are running we should return to the client right away.
    if (no_plugins) {
        soup_server_unpause_message (server_data->server, client_msg);
        g_slice_free (ServerData, server_data);
    }
}

static void
server_recipe_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
    AppData *app_data = (AppData *) data;
    SoupMessage *server_msg;
    SoupURI *server_uri;

    ServerData *server_data = g_slice_new0 (ServerData);
    server_data->path = path;
    server_data->app_data = app_data;
    server_data->client_msg = client_msg;
    server_data->server = server;

    if (app_data->state == RECIPE_IDLE) {
        soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, "No Recipe Running");
        return;
    }
    // FIXME - make sure we have valid tasks first
    Task *task = (Task *) app_data->tasks->data;

    if (g_str_has_suffix (path, "/results/")) {
        server_uri = soup_uri_new_with_base (task->task_uri, "results/");
        server_msg = soup_message_new_from_uri ("POST", server_uri);
    } else if (g_strrstr (path, "/logs/") != NULL) {
        gchar *log_url = swap_base(path, soup_uri_to_string(task->task_uri, FALSE), "/recipes/");
        server_uri = soup_uri_new (log_url);
        g_free (log_url);
        server_msg = soup_message_new_from_uri ("PUT", server_uri);
    } else if (g_str_has_suffix (path, "watchdog")) {
        // This does *not* update the localwatchdog.
        server_uri = soup_uri_new_with_base (task->recipe->recipe_uri, "watchdog");
        server_msg = soup_message_new_from_uri ("POST", server_uri);
    } else {
        soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, "No Match, Invalid request");
        return;
    }

    soup_uri_free (server_uri);
    soup_message_headers_foreach (client_msg->request_headers, copy_header,
                                  server_msg->request_headers);
    if (client_msg->request_body->length) {
      SoupBuffer *request = soup_message_body_flatten (client_msg->request_body);
      soup_message_body_append_buffer (server_msg->request_body, request);
      soup_buffer_free (request);
    }
    restraint_queue_message (soup_session, server_msg, server_msg_complete, server_data);
    soup_server_pause_message (server, client_msg);
}

static void
server_address_callback (SoupServer *server, SoupMessage *client_msg,
                     const char *path, GHashTable *query,
                     SoupClientContext *context, gpointer data)
{
  const gchar *address = soup_client_context_get_host (context);
  soup_message_headers_append (client_msg->response_headers, "Address", address);
  soup_message_set_status (client_msg, SOUP_STATUS_OK);
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

  // Only accept POST requests for running recipes
  if (client_msg->method != SOUP_METHOD_POST ) {
    soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, "only POST accepted");
    return;
  }

  // decode data from request_body
  table = soup_form_decode(client_msg->request_body->data);
  recipe_url = g_strdup_printf ("%s", (gchar *)g_hash_table_lookup (table, "recipe"));

  // Attempt to run a recipe if requested.
  // if the same url is requested we ignore it and continue running.
  if (recipe_url) {
    if (g_strcmp0 (recipe_url, app_data->recipe_url) == 0) {
      gchar *message = "* Continuing recipe\n";
      soup_message_body_append (client_msg->response_body, SOUP_MEMORY_COPY,
                                message, strlen(message));
      soup_message_set_status (client_msg, SOUP_STATUS_OK);
      return;
    } else {
      ServerData *server_data = g_slice_new0 (ServerData);
      server_data->path = path;
      server_data->app_data = app_data;
      server_data->client_msg = client_msg;
      server_data->server = server;

      if (!handle_recipe_request (recipe_url, server_data, &error)) {
        g_warning(error->message);
        soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, error->message);
        g_error_free (error);
        return;
      }
    }
  } else {
      soup_message_set_status_full (client_msg, SOUP_STATUS_BAD_REQUEST, "Unrecognized Command");
      return;
  }
  soup_message_headers_set_encoding (client_msg->response_headers,
                                     SOUP_ENCODING_CHUNKED);
  soup_message_set_status (client_msg, SOUP_STATUS_OK);
}

void term(int signum) {
  printf("[*] Caught %d, stopping mainloop\n", signum);
  g_main_loop_quit(loop);
}

int main(int argc, char *argv[]) {
  AppData *app_data = g_slice_new0(AppData);
  gint port = 8081;
  app_data->config_file = NULL;
  gchar *config_port = "config.conf";
  SoupServer *soup_server_ipv4 = NULL;
  SoupServer *soup_server_ipv6 = NULL;
  GError *error = NULL;

  GOptionEntry entries [] = {
        { "port", 'p', 0, G_OPTION_ARG_INT, &port,
            "Use the following config file", "CONFIGFILE" },
        { NULL }
  };
  GOptionContext *context = g_option_context_new(NULL);
  g_option_context_set_summary(context,
          "Test harness for Beaker. Runs tasks according to a recipe.");
  g_option_context_add_main_entries(context, entries, NULL);
  gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &app_data->error);
  g_option_context_free(context);

  if (!parse_succeeded) {
    exit (1);
  }

  // port option passed in.
  if (port != 8081) {
      config_port = g_strdup_printf ("config_%d.conf", port);
  }
  app_data->restraint_url = g_strdup_printf ("http://localhost:%d", port);
  app_data->config_file = g_build_filename (VAR_LIB_PATH, config_port, NULL);

  app_data->recipe_url = restraint_config_get_string (app_data->config_file,
                                                      "restraint",
                                                      "recipe_url", &error);
  if (error) {
      g_printerr ("%s [%s, %d]\n", error->message,
                  g_quark_to_string (error->domain), error->code);
      g_clear_error(&error);
      exit (1);
  }

  if (app_data->recipe_url) {
    ServerData *server_data = g_slice_new0 (ServerData);
    server_data->app_data = app_data;
    app_data->state = RECIPE_FETCH;
    app_data->recipe_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                  recipe_handler,
                                                  server_data,
                                                  recipe_handler_finish);
  }

  soup_session = soup_session_new();
  soup_session_add_feature_by_type (soup_session, SOUP_TYPE_CONTENT_SNIFFER);

  SoupAddress *addr_ipv6 = soup_address_new_any (SOUP_ADDRESS_FAMILY_IPV6,
                                                 port);
  soup_server_ipv6 = soup_server_new (SOUP_SERVER_INTERFACE, addr_ipv6, NULL);
  g_object_unref (addr_ipv6);

  // IPv6
  guint ipv6_only = 1, len = sizeof (ipv6_only);

  if (soup_server_ipv6) {
      // get the socket file descriptor
      gint socket_fd = soup_socket_get_fd (soup_server_get_listener (soup_server_ipv6));
      // current libsoup defaults to GSocket defaults, this means we could
      // get a dual ipv4/ipv6 socket.  We need to check and only open an ipv4
      // socket if not.
      if (getsockopt (socket_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                      (gpointer)&ipv6_only, (gpointer)&len) == -1) {
          g_printerr ("Can't determine if IPV6_V6ONLY is set!\n");
      }

      // close soup_server socket on exec
      fcntl (socket_fd, F_SETFD, FD_CLOEXEC);

      // Add the handlers
      soup_server_add_handler (soup_server_ipv6, "/address",
                               server_address_callback, app_data, NULL);
      soup_server_add_handler (soup_server_ipv6, "/control",
                               server_control_callback, app_data, NULL);
      soup_server_add_handler (soup_server_ipv6, "/recipes",
                               server_recipe_callback, app_data, NULL);
      // Run the server
      soup_server_run_async (soup_server_ipv6);
  }

  // IPv4
  if (ipv6_only == 1) {
      SoupAddress *addr_ipv4 = soup_address_new_any (SOUP_ADDRESS_FAMILY_IPV4,
                                                     port);
      soup_server_ipv4 = soup_server_new (SOUP_SERVER_INTERFACE, addr_ipv4, NULL);
      g_object_unref (addr_ipv4);

      // close soup_server socket on exec
      gint socket_fd = soup_socket_get_fd (soup_server_get_listener (soup_server_ipv4));
      fcntl (socket_fd, F_SETFD, FD_CLOEXEC);

      // Add the handlers
      soup_server_add_handler (soup_server_ipv4, "/address",
                               server_address_callback, app_data, NULL);
      soup_server_add_handler (soup_server_ipv4, "/control",
                               server_control_callback, app_data, NULL);
      soup_server_add_handler (soup_server_ipv4, "/recipes",
                               server_recipe_callback, app_data, NULL);
      // Run the server
      soup_server_run_async (soup_server_ipv4);
  }

  if (!soup_server_ipv4 && !soup_server_ipv6) {
      g_printerr ("Unable to bind to server port %d\n", port);
      exit (1);
  }

  signal(SIGINT, &term);
  signal(SIGTERM, &term);

  /* enter mainloop */
  g_print ("Waiting for client!\n");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  soup_session_abort(soup_session);
  soup_session_remove_feature_by_type (soup_session, SOUP_TYPE_CONTENT_SNIFFER);
  g_object_unref(soup_session);

  soup_server_disconnect(soup_server_ipv6);
  g_object_unref(soup_server_ipv6);
  if (ipv6_only == 1) {
    soup_server_disconnect(soup_server_ipv4);
    g_object_unref(soup_server_ipv4);
  }

  restraint_free_app_data(app_data);

  g_main_loop_unref(loop);

  return 0;
}

