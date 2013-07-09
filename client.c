#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#ifndef G_OS_WIN32 /* ! HAVE_GIO_UNIX */
#include <gio/gunixsocketaddress.h>
#endif

typedef struct {
    GMainLoop *loop;
} AppData;

static gboolean io_callback(GIOChannel *io, GIOCondition condition, gpointer user_data) {
    AppData *app_data = (AppData *) user_data;
    gchar *s = NULL;
    gsize len = 0;
    GError *error = NULL;

    /* message format 
     * ---------------------
     * RSTRNT/1.0
     * Command: CMD
     * Content-length: 1023
     *
     * Data....
     * ---------------------
     */
    switch (g_io_channel_read_line(io, &s, &len, NULL, &error)) {

        case G_IO_STATUS_NORMAL:
            write(STDOUT_FILENO, s, (size_t) len);
            return TRUE;

        case G_IO_STATUS_ERROR:
            g_printerr ("IO error: %s\n", error->message);
            g_error_free (error);
            return FALSE;

        case G_IO_STATUS_EOF:
            g_main_loop_quit(app_data->loop);
            return FALSE;

        case G_IO_STATUS_AGAIN:
            g_warning("not ready, try again!\n");
            return TRUE;

        default:
            g_return_val_if_reached (FALSE);
            break;
    }
    return FALSE;
}

int main(int argc, char *argv[]) {

    g_type_init();

    AppData data = { NULL, };
    GError *error = NULL;
    gchar *recipe_run = NULL;
    gboolean recipe_verbose = FALSE;
    gboolean recipe_cancel = FALSE;
    int ret = EXIT_FAILURE;
    //guint io_watch_id = 0;

    /* create a new connection */
    GIOChannel *io = NULL;
    GSocketConnection *connection = NULL;
    GSocketAddress *address = NULL;
    GSocketClient *client = g_socket_client_new();
    GSocket *socket = NULL;

    data.loop = g_main_loop_new(NULL, FALSE);

    GOptionEntry entries[] = {
        { "run", 0, 0, G_OPTION_ARG_STRING, &recipe_run,
            "Run recipe from URL or file", "URL" },
        { "verbose", 0, 0, G_OPTION_ARG_NONE, &recipe_verbose,
            "Connect to restraintd and watch running recipe", NULL },
        { "cancel", 0, 0, G_OPTION_ARG_NONE, &recipe_cancel,
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

#ifdef G_OS_WIN32 /* HAVE_GIO_UNIX */
    GInetAddress *inet_address;
    guint16 port = 1500;
    inet_address = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6);
    address = g_inet_socket_address_new(inet_address, port);
    g_object_unref(inet_address);
#else
    const gchar *socket_local = "/tmp/restraint.socket";
    address = g_unix_socket_address_new(socket_local);
#endif /* HAVE_GIO_UNIX */

    connection = g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, &error);
    if (error != NULL)
        goto cleanup;

    socket = g_socket_connection_get_socket(connection);

    /* use the connection */
    GOutputStream *ostream = g_io_stream_get_output_stream(G_IO_STREAM(connection));
    //GInputStream *istream = g_io_stream_get_input_stream(G_IO_STREAM(connection));

    g_print("recipe_run=%s\n", recipe_run);
    gchar *newline = g_strdup_printf("%s\n", recipe_run);
    g_output_stream_write(ostream, newline, strlen(newline), NULL, &error);
    if (error != NULL) {
        goto cleanup;
    }

    io = g_io_channel_unix_new(g_socket_get_fd(socket));
    g_io_add_watch(io, G_IO_IN, io_callback, &data);
    g_io_channel_unref(io);

    g_message("Running...");
    g_main_loop_run(data.loop);
    g_message("Returned");

    ret = EXIT_SUCCESS;

cleanup:
    if (data.loop)
        g_main_loop_unref(data.loop);
    if (error)
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
    return ret;
}
