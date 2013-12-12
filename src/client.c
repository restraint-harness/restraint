#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libsoup/soup.h>
#include "common.h"

#define READ_BUFFER_SIZE 8192
static SoupSession *session;

typedef struct {
    GError *error;
    StreamType stream_type; // harness data on one stream, testout on another, and error on another.
    gint stream_error_code; // error code reported from haness.
    gchar buffer[READ_BUFFER_SIZE]; // buffer used with g_input_stream
    GString *output; // seems redundant with buffer.
    SoupMultipartInputStream *multipart;
    GMainLoop *loop;
} AppData;

#define RESTRAINT_CLIENT_ERROR restraint_client_error()
GQuark
restraint_client_error(void) {
    return g_quark_from_static_string("restraint-client-error");
}
#define RESTRAINT_CLIENT_STREAM_ERROR restraint_client_stream_error()
GQuark
restraint_client_stream_error(void) {
    return g_quark_from_static_string("restraint-client-stream-error");
}

/*
 * Check the status of closing the input_stream
 */
static void
multipart_close_part_cb (GObject *source, 
                         GAsyncResult *async_result,
                         gpointer data)
{
    AppData *app_data = (AppData *) data;
    GInputStream *in = G_INPUT_STREAM (source);

    g_input_stream_close_finish (in, async_result, &app_data->error);
    if (app_data->error) {
        g_main_loop_quit (app_data->loop);
        return;
    }
}

static void multipart_next_part_cb (GObject *source,
                                    GAsyncResult *async_result,
                                    gpointer data);

/*
 * Read from g_input_stream until we get 0 bytes read.  Then process
 * using the value of stream_type.  Finally try and read another multipart.
 */
static void
multipart_read_cb (GObject *source, GAsyncResult *async_result, gpointer data)
{
    AppData *app_data = (AppData *) data;
    GInputStream *in = G_INPUT_STREAM (source);
    gssize bytes_read;

    bytes_read = g_input_stream_read_finish (in, async_result, &app_data->error);
    if (app_data->error) {
        g_main_loop_quit (app_data->loop);
        return;
    }
    app_data->output = g_string_append_len (app_data->output, app_data->buffer, bytes_read);
    /* Read 0 bytes - try to start reading another part. */
    if (!bytes_read) {
        switch (app_data->stream_type) {
          case STREAM_STDOUT:
            write(STDOUT_FILENO, app_data->output->str, app_data->output->len);
            break;
          case STREAM_STDERR:
            write(STDERR_FILENO, app_data->output->str, app_data->output->len);
            break;
          case STREAM_ERROR:
            g_set_error_literal(&app_data->error, RESTRAINT_CLIENT_STREAM_ERROR,
                                app_data->stream_error_code, app_data->output->str);
            g_main_loop_quit (app_data->loop);
            return;
            break;
        }
        g_input_stream_close_async (in, G_PRIORITY_DEFAULT, NULL,
                                    multipart_close_part_cb, data);
        g_object_unref (in);
        soup_multipart_input_stream_next_part_async (app_data->multipart, G_PRIORITY_DEFAULT,
                                                     NULL, multipart_next_part_cb,
                                                     data);
        return;
    }
    g_input_stream_read_async (in, app_data->buffer, READ_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT, NULL, multipart_read_cb, data);
}

/*
 * We have two headeers we care about, Content-ID which we use as steam_type and
 * Status which we read when stream_type equals STREAM_ERROR.
 */
static void
multipart_read_headers (AppData *app_data)
{
    const gchar *value = NULL;
    SoupMessageHeaders *headers;

    headers = soup_multipart_input_stream_get_headers (app_data->multipart);
    value = soup_message_headers_get_one (headers, "Content-ID");
    if (value)
        app_data->stream_type = g_ascii_strtoull (value, NULL, 0);
    value = soup_message_headers_get_one (headers, "Status");
    if (value)
        app_data->stream_error_code = g_ascii_strtoull (value, NULL, 0);
}

/*
 * Try and read another multipart message. if in is NULL then there are no more
 * messages to read.
 */
static void
multipart_next_part_cb (GObject *source, GAsyncResult *async_result, gpointer data)
{
    AppData *app_data = (AppData *) data;
    GInputStream *in;
    gsize read_size = READ_BUFFER_SIZE;

    g_assert (SOUP_MULTIPART_INPUT_STREAM (source) == app_data->multipart);

    in = soup_multipart_input_stream_next_part_finish (app_data->multipart, async_result, &app_data->error);
    if (app_data->error) {
        g_main_loop_quit (app_data->loop);
        return;
    }

    if (!in) {
        g_object_unref (app_data->multipart);
        g_main_loop_quit (app_data->loop);
        return;
    }

    // Read the headers here.
    multipart_read_headers (app_data);

    if (app_data->output)
        g_string_free (app_data->output, TRUE);
    app_data->output = g_string_sized_new(READ_BUFFER_SIZE);

    g_input_stream_read_async (in, app_data->buffer, read_size,
                               G_PRIORITY_DEFAULT, NULL,
                               multipart_read_cb, data);
}

/*
 * our multipart handler callback
 * If we make an invalid request (like trying to cancel when no recipe is running)
 * then status_code will not be 200 and we will exit out after propagating the error
 * to our own GError
 */
static void
multipart_handling_cb (GObject *source, GAsyncResult *async_result, gpointer data)
{
    AppData *app_data = (AppData *) data;
    SoupRequest *request = SOUP_REQUEST (source);
    GInputStream *in;
    SoupMessage *message;

    in = soup_request_send_finish (request, async_result, &app_data->error);
    if (app_data->error) {
        g_main_loop_quit (app_data->loop);
        return;
    }
    message = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));

    if (message->status_code != SOUP_STATUS_OK) {
      g_set_error_literal(&app_data->error, RESTRAINT_CLIENT_ERROR, message->status_code, message->reason_phrase);
      g_main_loop_quit (app_data->loop);
      return;
    }

    app_data->multipart = soup_multipart_input_stream_new (message, in);
    g_object_unref (message);
    g_object_unref (in);

    soup_multipart_input_stream_next_part_async (app_data->multipart, G_PRIORITY_DEFAULT, NULL,
                                                 multipart_next_part_cb, data);
}

int main(int argc, char *argv[]) {

    gchar *server = "http://localhost:8081"; // Replace with a unix socket proxy so no network is required
                                             // when run from localhost.
    gchar *recipe_run = NULL;
    gboolean recipe_monitor = FALSE;
    gboolean recipe_cancel = FALSE;
    gboolean true = TRUE; // Is there a better way to pass the address of a boolean? required for ghashtable.

    SoupURI *server_uri;
    SoupURI *control_uri;

    SoupMessage *server_msg;
    SoupRequest *request;

    gchar *form_data;
    GHashTable *data_table = NULL;
    AppData *app_data = g_slice_new0 (AppData);

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
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Test harness for Beaker. Runs tasks according to a recipe \n"
            "and collects their results.");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &app_data->error);
    g_option_context_free(context);

    if (!parse_succeeded) {
        goto cleanup;
    }

    if (!recipe_run && !recipe_monitor && !recipe_cancel) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    app_data->loop = g_main_loop_new(NULL, FALSE);
    server_uri = soup_uri_new (server);
    session = soup_session_new_with_options("timeout", 310, NULL);

    data_table = g_hash_table_new (NULL, NULL);
    if (recipe_run)
      g_hash_table_insert (data_table, "recipe", recipe_run);
    if (recipe_monitor || recipe_cancel)
      g_hash_table_insert (data_table, "monitor", &true);
    if (recipe_cancel)
      g_hash_table_insert (data_table, "cancel", &recipe_cancel);

    control_uri = soup_uri_new_with_base (server_uri, "control");
    request = (SoupRequest *)soup_session_request_http_uri (session, "POST", control_uri, &app_data->error);
    server_msg = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    soup_uri_free (control_uri);
    form_data = soup_form_encode_hash (data_table);
    soup_message_set_request (server_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));
    soup_request_send_async (request, NULL, multipart_handling_cb, app_data);
    g_main_loop_run(app_data->loop);

cleanup:
    if (app_data->error) {
        g_printerr("%s [%s, %d]\n", app_data->error->message,
                g_quark_to_string(app_data->error->domain), app_data->error->code);
        return app_data->error->code;
    } else {
        return EXIT_SUCCESS;
    }
}
