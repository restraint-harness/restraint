#include <glib.h>
#include <libsoup/soup.h>
#include "multipart.h"
#include "errors.h"

static void next_part_cb (GObject *source, GAsyncResult *async_result, gpointer user_data);

static void
multipart_destroy (MultiPartData *multipart_data)
{
    //GError *error = NULL;
    //g_print ("multipart_destroy: Enter\n");
    if (multipart_data->destroy) {
        multipart_data->destroy (multipart_data->error,
                                 multipart_data->user_data);
    }
    g_clear_error (&multipart_data->error);
    g_slice_free (MultiPartData, multipart_data);
    //g_print ("multipart_destroy: Exit\n");
}

static void
close_cb (GObject *source,
          GAsyncResult *async_result,
          gpointer user_data)
{
    GError *error = NULL;
    g_input_stream_close_finish (G_INPUT_STREAM (source), async_result, &error);
    if (error) {
        g_printerr("close_cb: %s [%s, %d]\n", error->message,
                   g_quark_to_string(error->domain),
                   error->code);
        g_error_free (error);
    }
}

static void
close_base_cb (GObject *source,
          GAsyncResult *async_result,
          gpointer user_data)
{
    MultiPartData *multipart_data = (MultiPartData *) user_data;
    GError *error = NULL;
    g_input_stream_close_finish (G_INPUT_STREAM (source), async_result, &error);
    if (error) {
        g_printerr("close_base_cb: %s [%s, %d]\n", error->message,
                   g_quark_to_string(error->domain),
                   error->code);
        g_error_free (error);
    }
    multipart_destroy (multipart_data);
}

/*
 *  * Read from g_input_stream until we get 0 bytes read.  Then process
 *   * using the value of stream_type.  Finally try and read another multipart.
 *    */
static void
read_cb (GObject *source, GAsyncResult *async_result, gpointer user_data)
{
    //g_print ("read_cb: Enter\n");
    GInputStream *in = G_INPUT_STREAM (source);
    MultiPartData *multipart_data = (MultiPartData *) user_data;
    SoupMultipartInputStream *multipart = SOUP_MULTIPART_INPUT_STREAM (multipart_data->multipart);
    gssize bytes_read;

    bytes_read = g_input_stream_read_finish (in, async_result, &multipart_data->error);

    /* Read 0 bytes - try to start reading another part. */
    if (bytes_read <= 0) {
        g_input_stream_close_async (in,
                                    G_PRIORITY_DEFAULT,
                                    multipart_data->cancellable,
                                    close_cb,
                                    user_data);
        if (multipart_data->callback) {
            SoupBuffer *soup_buffer;
            //g_print ("callback\n");
            soup_buffer = soup_buffer_new(SOUP_MEMORY_TEMPORARY,
                                (guchar *) multipart_data->buffer->str,
                                multipart_data->buffer->len);
            multipart_data->callback (multipart_data->method,
                                      multipart_data->path,
                                      multipart_data->cancellable,
                                      multipart_data->error,
                                      multipart_data->headers,
                                      soup_buffer,
                                      multipart_data->user_data);
            soup_buffer_free(soup_buffer);
        }
        g_string_free (multipart_data->buffer, TRUE);
        if (multipart_data->error) {
            g_input_stream_close_async (G_INPUT_STREAM (multipart),
                                        G_PRIORITY_DEFAULT,
                                        multipart_data->cancellable,
                                        close_base_cb,
                                        user_data);
            return;
        }
        soup_multipart_input_stream_next_part_async (multipart_data->multipart,
                                                     G_PRIORITY_DEFAULT,
                                                     multipart_data->cancellable,
                                                     next_part_cb,
                                                     user_data);
        return;
    }
    multipart_data->buffer = g_string_append_len (multipart_data->buffer, multipart_data->read_buffer, bytes_read);
    g_input_stream_read_async (in,
                               multipart_data->read_buffer,
                               READ_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT,
                               multipart_data->cancellable,
                               read_cb,
                               user_data);
    //g_print ("read_cb: Exit\n");
}

/*
 *  * We have two headeers we care about, Content-ID which we use as steam_type and
 *   * Status which we read when stream_type equals STREAM_ERROR.
 *    */
static void
multipart_read_headers (MultiPartData *multipart_data)
{
    multipart_data->headers = soup_multipart_input_stream_get_headers (multipart_data->multipart);
    if (multipart_data->headers) {
        multipart_data->method = soup_message_headers_get_one (multipart_data->headers, "rstrnt-method");
        multipart_data->path = soup_message_headers_get_one (multipart_data->headers, "rstrnt-path");
    }
}

/*
 * Try and read another multipart message. if in is NULL then there are no more
 * messages to read.
 */
static void
next_part_cb (GObject *source, GAsyncResult *async_result, gpointer user_data)
{
    //g_print ("next_part_cb: Enter\n");
    SoupMultipartInputStream *multipart = SOUP_MULTIPART_INPUT_STREAM (source);

    MultiPartData *multipart_data = (MultiPartData *) user_data;

    g_assert (SOUP_MULTIPART_INPUT_STREAM (source) == multipart_data->multipart);

    GInputStream *in = soup_multipart_input_stream_next_part_finish (multipart,
                                                       async_result,
                                                       &multipart_data->error);
    if (!in) {
        g_input_stream_close_async (G_INPUT_STREAM (multipart),
                                    G_PRIORITY_DEFAULT,
                                    multipart_data->cancellable,
                                    close_base_cb,
                                    user_data);
        return;
    }

    // Read the headers here.
    multipart_read_headers (multipart_data);

    multipart_data->buffer = g_string_sized_new(READ_BUFFER_SIZE);

    g_input_stream_read_async (in,
                               multipart_data->read_buffer,
                               READ_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT,
                               multipart_data->cancellable,
                               read_cb,
                               user_data);
    //g_print ("next_part_cb: Exit\n");
}

/*
 * our multipart handler callback
 * If we make an invalid request (like trying to cancel when no recipe is running)
 * then status_code will not be 200 and we will exit out after propagating the error
 * to our own GError
 */
static void
request_sent_cb (GObject *source, GAsyncResult *async_result, gpointer user_data)
{

    //g_print ("request_sent_cb\n");
    MultiPartData *multipart_data = (MultiPartData *) user_data;

    SoupRequest *request = SOUP_REQUEST (source);
    GInputStream *in = soup_request_send_finish (request,
                                                 async_result,
                                                 &multipart_data->error);
    if (multipart_data->error) {
        g_object_unref(request);
        multipart_destroy (multipart_data);
        return;
    }

    SoupMessage *message = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    g_object_unref(request);

    if (message->status_code != SOUP_STATUS_OK) {
        g_set_error_literal(&multipart_data->error,
                            RESTRAINT_ERROR,
                            message->status_code,
                            message->reason_phrase);
        multipart_destroy (multipart_data);
        return;
    }

    multipart_data->multipart = soup_multipart_input_stream_new (message,
                                                                 in);
    g_object_unref (message);
    g_object_unref (in);

    soup_multipart_input_stream_next_part_async (multipart_data->multipart,
                                                 G_PRIORITY_DEFAULT,
                                                 multipart_data->cancellable,
                                                 next_part_cb,
                                                 user_data);
}

void
multipart_request_send_async (SoupRequest *request,
                              GCancellable *cancellable,
                              MultiPartCallback callback,
                              MultiPartDestroy destroy,
                              gpointer user_data)
{
    MultiPartData *multipart_data = g_slice_new0 (MultiPartData);
    multipart_data->callback = callback;
    multipart_data->destroy = destroy;
    multipart_data->user_data = user_data;
    multipart_data->cancellable = cancellable;
    multipart_data->buffer = NULL;

    soup_request_send_async(request, cancellable, request_sent_cb, multipart_data);
}
