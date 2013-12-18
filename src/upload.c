#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#define READ_BUFFER_SIZE 8192
static gchar input_buf[READ_BUFFER_SIZE];
static gssize offset = 0;

static gint
upload_chunk (SoupSession *session,
              GInputStream *in,
              gulong filesize,
              SoupURI *result_log_uri,
              GError **error)
{
    SoupMessage *server_msg;
    gchar *range;
    gssize bytes_read;
    gint ret;
    GError *tmp_error = NULL;
    bytes_read = g_input_stream_read (in, input_buf, READ_BUFFER_SIZE, NULL, &tmp_error);
    if (tmp_error) {
        g_propagate_error (error, tmp_error);
        return -1;
    }
    if (bytes_read > 0) {
        server_msg = soup_message_new_from_uri ("PUT", result_log_uri);
        range = g_strdup_printf ("bytes %zu-%zu/%lu",
                                 offset,
                                 offset + bytes_read - 1,
                                 filesize);
        offset += bytes_read;
        soup_message_headers_append (server_msg->request_headers, "Content-Range", range);
        g_free (range);
        soup_message_set_request (server_msg, "text/plain", SOUP_MEMORY_COPY, input_buf, bytes_read);
        ret = soup_session_send_message (session, server_msg);
        g_object_unref (server_msg);   
        if (SOUP_STATUS_IS_SUCCESSFUL (ret)) {
            return 1;
        } else {
            g_set_error_literal (error, SOUP_HTTP_ERROR, server_msg->status_code,
                                     server_msg->reason_phrase);
            return -1;
        }
    }
    return 0; 
}

gboolean
upload_file (SoupSession *session,
             gchar *filepath,
             gchar *filename,
             SoupURI *results_uri,
             GError **error)
{
    GFile *f = g_file_new_for_path (filepath);
    GFileInputStream *fis = NULL;
    GFileInfo *fileinfo = NULL;
    gulong filesize;
    GError *tmp_error = NULL;
    SoupURI *result_log_uri;
    gint ret;

    fileinfo = g_file_query_info (f, "standard::*", G_FILE_QUERY_INFO_NONE, NULL, &tmp_error);
    if (tmp_error != NULL) {
        g_propagate_prefixed_error (error, tmp_error,
            "Error querying: %s ", filepath);
        g_object_unref (f);
        return FALSE;
    }

    filesize = g_file_info_get_attribute_uint64 (fileinfo, G_FILE_ATTRIBUTE_STANDARD_SIZE);

    /* get input stream */
    fis = g_file_read (f, NULL, &tmp_error);

    if (tmp_error != NULL) {
        g_propagate_prefixed_error (error, tmp_error,
            "Error opening: %s ", filepath);
        g_object_unref (f);
        return FALSE;
    }

    result_log_uri = soup_uri_new_with_base (results_uri, filename);
    while ((ret = upload_chunk (session, G_INPUT_STREAM (fis), filesize, result_log_uri, &tmp_error)) > 0) {
        // replace with callback
        g_print (".");
    }
    soup_uri_free (result_log_uri);
    g_object_unref(fis);

    if (ret == 0)
        return TRUE;
    else
        return FALSE;
}
