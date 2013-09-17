#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libsoup/soup.h>

static SoupSession *session;
static GMainLoop *loop;

static void
write_chunk (SoupMessage *from, SoupBuffer *chunk, gpointer data)
{
    write(STDOUT_FILENO, chunk->data, (size_t)chunk->length);
}

static void
finish_msg (SoupSession *session, SoupMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    g_main_loop_quit(loop);
    g_main_loop_unref(loop);
}

int main(int argc, char *argv[]) {

    GError *error = NULL;
    gchar *server = "http://localhost:8081";
    gchar *recipe_run = NULL;
    gboolean recipe_monitor = FALSE;
    gboolean recipe_cancel = FALSE;
    SoupURI *server_uri;
    SoupURI *control_uri;
    SoupMessage *msg;
    gchar *data;

    int ret = EXIT_FAILURE;

    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { "run", 'r', 0, G_OPTION_ARG_STRING, &recipe_run,
            "Run recipe from URL or file", "URL" },
        { "monitor", 'm', 0, G_OPTION_ARG_NONE, &recipe_monitor,
            "Monitor running recipe", NULL },
        { "cancel", 'c', 0, G_OPTION_ARG_NONE, &recipe_cancel,
            "Cancel running recipe", NULL },
        { NULL }
    };
    GOptionContext *context = g_option_context_new("");
    g_option_context_set_summary(context,
            "Test harness for Beaker. Runs tasks according to a recipe \n"
            "and collects their results.");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);
    if (!parse_succeeded)
        goto cleanup;

    loop = g_main_loop_new(NULL, FALSE);
    server_uri = soup_uri_new (server);
    session = soup_session_new_with_options("timeout", 0, NULL);

    if (recipe_run) {
      control_uri = soup_uri_new_with_base (server_uri, "control/run");
      msg = soup_message_new_from_uri ("POST", control_uri);
      soup_uri_free (control_uri);
      data = soup_form_encode ("recipe", recipe_run, NULL);
      soup_message_set_request (msg, "application/x-www-form-urlencoded",
                                SOUP_MEMORY_TAKE, data, strlen (data));
      soup_session_send_message (session, msg);
      if (msg->status_code != SOUP_STATUS_OK) {
        g_printerr ("Unexpected response: %d %s\n",
                    msg->status_code, msg->reason_phrase);
      }
    }

    if (recipe_monitor) {
      control_uri = soup_uri_new_with_base (server_uri, "control/monitor");
      msg = soup_message_new_from_uri ("GET", control_uri);
      soup_uri_free (control_uri);

      g_signal_connect (msg, "got_chunk", G_CALLBACK (write_chunk), NULL);

      soup_session_queue_message (session, msg, finish_msg, loop);
      g_main_loop_run(loop);
    }

    if (recipe_cancel) {
      control_uri = soup_uri_new_with_base (server_uri, "control/cancel");
      msg = soup_message_new_from_uri ("POST", control_uri);
      soup_uri_free (control_uri);
      soup_session_send_message (session, msg);
      if (msg->status_code != SOUP_STATUS_OK) {
        g_printerr ("Unexpected response: %d %s\n",
                    msg->status_code, msg->reason_phrase);
      }
    }

    ret = EXIT_SUCCESS;

cleanup:
    if (error)
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
    return ret;
}
