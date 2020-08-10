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

#define LOG_MANAGER_DIR "./test_logging_logs"

#include "logging.c"

SoupSession *soup_session;

static int message_callback_calls = 0;

static void
check_log_file_contents (const RstrntTask *task,
                         const char       *child,
                         const char       *contents)
{
    g_autofree char *path = NULL;
    g_autoptr (GFile) log_directory_file = NULL;
    g_autoptr (GFile) child_file = NULL;
    g_autofree char *child_path = NULL;
    g_autoptr (GMappedFile) mapped_file = NULL;
    size_t file_length;
    char *file_contents;
    size_t contents_length;

    path = g_build_path ("/", LOG_MANAGER_DIR, task->task_id, NULL);
    log_directory_file = g_file_new_for_path (path);
    child_file = g_file_get_child (log_directory_file, child);
    child_path = g_file_get_path (child_file);
    mapped_file = g_mapped_file_new (child_path, FALSE, NULL);
    file_length = g_mapped_file_get_length (mapped_file);
    file_contents = g_mapped_file_get_contents (mapped_file);
    contents_length = strlen (contents);

    g_assert_cmpuint (contents_length, ==, file_length);
    g_assert_cmpstr (contents, ==, file_contents);
}

static void
test_rstrnt_log_write (gconstpointer user_data)
{
    const RstrntTask *task;
    g_autofree char *task_contents = NULL;
    g_autofree char *harness_contents = NULL;

    task = user_data;
    task_contents = g_strdup_printf ("el task");
    harness_contents = g_strdup_printf ("el harness");

    rstrnt_log (task, RSTRNT_LOG_TYPE_TASK, "%s", task_contents);
    rstrnt_log (task, RSTRNT_LOG_TYPE_HARNESS, "%s", harness_contents);

    rstrnt_flush_logs (task, NULL);

    check_log_file_contents (task, "task.log", task_contents);
    check_log_file_contents (task, "harness.log", harness_contents);
}

static void
server_callback (SoupServer        *server,
                 SoupMessage       *msg,
                 const char        *path,
                 GHashTable        *query,
                 SoupClientContext *client,
                 gpointer           user_data)
{
    static bool task_log_handled = false;
    static bool harness_log_handled = false;

    g_assert_cmpstr (msg->method, ==, SOUP_METHOD_PUT);

    if (RSTRNT_LOG_TYPE_TASK == GPOINTER_TO_INT (user_data))
    {
        g_assert_false (task_log_handled);
        task_log_handled = true;

        soup_server_remove_handler (server, "/" LOG_PATH_TASK);
    }
    else if (RSTRNT_LOG_TYPE_HARNESS == GPOINTER_TO_INT (user_data))
    {
        g_assert_false (harness_log_handled);
        harness_log_handled = true;

        soup_server_remove_handler (server, "/" LOG_PATH_HARNESS);
    }

    soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void
message_callback (SoupSession *session,
                  SoupMessage *msg,
                  gpointer     user_data)
{
    MessageData *message_data;

    g_assert_cmpint (msg->status_code, ==, SOUP_STATUS_OK);
    g_assert_nonnull (user_data);

    message_data = (MessageData *) user_data;

    g_assert_nonnull (message_data->finish_callback);

    /* Call to rstrnt_on_log_uploaded() */
    g_assert_true (message_data->finish_callback == rstrnt_on_log_uploaded);
    message_data->finish_callback (session, msg, message_data->user_data);

    g_slice_free (MessageData, message_data);

    message_callback_calls++;
}

static void
queue_message (SoupSession           *session,
               SoupMessage           *msg,
               gpointer               msg_data,
               MessageFinishCallback  finish_callback,
               GCancellable          *cancellable,
               gpointer               user_data)
{
    MessageData *message_data;

    message_data = g_slice_new0 (MessageData);
    message_data->session = session;
    message_data->msg = msg;
    message_data->user_data = user_data;
    message_data->finish_callback = finish_callback;

    soup_session_queue_message (session, msg, message_callback, message_data);
}

static void
test_rstrnt_log_upload (gconstpointer user_data)
{
    const RstrntTask *task;
    RstrntServerAppData app_data;
    int expected_calls;

    task = user_data;

    app_data.queue_message = queue_message;
    app_data.config_file = LOG_MANAGER_DIR "/config.conf";

    rstrnt_upload_logs (task, &app_data, soup_session, NULL);

    expected_calls = 2;

    while (message_callback_calls < expected_calls)
        g_main_context_iteration (NULL, TRUE);

    g_assert_cmpint (message_callback_calls, ==, expected_calls);

    message_callback_calls = 0;
}

static void
test_rstrnt_log_upload_no_logs (void)
{
    RstrntTask          *task;
    RstrntServerAppData  app_data;

    task = restraint_task_new ();
    task->task_id = g_strdup_printf ("%" G_GINT64_FORMAT, g_get_real_time ());

    app_data.queue_message = NULL;  /* Ensures failure if queue_message is called */
    app_data.config_file = LOG_MANAGER_DIR "/config.conf";

    rstrnt_upload_logs (task, &app_data, soup_session, NULL);

    restraint_task_free (task);
}

static gchar *
assert_content_range (SoupMessage *msg,
                      goffset      expected_start,
                      gsize        expected_len)
{
    gboolean has_header;
    goffset start;
    goffset end;
    goffset len;
    GString *msg_content = NULL;
    g_autoptr (SoupBuffer) buffer = NULL;

    has_header = soup_message_headers_get_content_range (msg->request_headers,
                                                         &start,
                                                         &end,
                                                         &len);

    g_assert_true (has_header);

    buffer = soup_message_body_get_chunk (msg->request_body, 0);

    g_assert_nonnull (buffer);
    g_assert_cmpint (start, ==, expected_start);
    g_assert_cmpint (end, ==, expected_start + buffer->length - 1);
    g_assert_cmpint (len, ==, expected_len);

    g_assert_cmpint (buffer->length, ==, end - start + 1);

    msg_content = g_string_new_len (buffer->data, buffer->length);

    return g_string_free (msg_content, FALSE);
}

static void
test_rstrnt_chunk_log_zero_offset (void)
{
    g_autoptr (GString) actual_log = NULL;
    g_autoptr (SoupURI) uri = NULL;
    g_autofree SoupMessage **msgv = NULL;
    int msgc = 0;
    int expected_msgc;
    const char *content;
    int chunk_len;

    chunk_len = 10;
    content = "This strin" \
              "g should b" \
              "e 3 msgs";
    expected_msgc = 3;

    uri = soup_uri_new ("http://internets:8000");

    msgv = rstrnt_chunk_log (uri, content, 0, strlen (content), chunk_len, &msgc);

    g_assert_nonnull (msgv);
    g_assert_cmpint (msgc, ==, expected_msgc);

    actual_log = g_string_new (NULL);

    for (int i = 0; i < msgc; i++) {
        char *msg_content;
        goffset start;

        g_assert_nonnull (msgv[i]);
        g_assert_true (soup_message_headers_header_equals (msgv[i]->request_headers,
                                                           "log-level",
                                                           "2"));

        start = i * chunk_len;

        msg_content = assert_content_range (msgv[i], start, -1);

        g_assert_nonnull (msg_content);
        actual_log = g_string_append (actual_log, msg_content);

        g_free (msg_content);
        g_object_unref (msgv[i]);
    }

    g_assert_cmpstr (actual_log->str, ==, content);
}

static void
test_rstrnt_chunk_log_nonzero_offset (void)
{
    const gchar             *content;
    const gchar             *expected_chunk;
    g_autofree SoupMessage **msgv = NULL;
    g_autofree gchar        *msg_content = NULL;
    g_autoptr (SoupURI)      uri = NULL;
    goffset                  offset;
    gsize                    chunk_len;
    int                      msgc = 0;

    content = "aaa bbb ccc";
    offset = 8;
    chunk_len = 4;
    expected_chunk = "ccc";

    uri = soup_uri_new ("http://internets:8000");
    msgv = rstrnt_chunk_log (uri, content, offset, strlen (content), chunk_len, &msgc);

    g_assert_nonnull (msgv);
    g_assert_cmpint (msgc, ==, 1);
    g_assert_nonnull (msgv[0]);

    msg_content = assert_content_range (msgv[0], offset, -1);

    g_assert_cmpstr (msg_content, ==, expected_chunk);

    g_object_unref (msgv[0]);
}

static void
test_rstrnt_log_manager_enabled (void)
{
    AppData app_data;

    app_data.stdin = FALSE;
    app_data.uploader_interval = 0;

    g_assert_false (rstrnt_log_manager_enabled (&app_data));

    app_data.stdin = FALSE;
    app_data.uploader_interval = 15;

    g_assert_true (rstrnt_log_manager_enabled (&app_data));

    app_data.stdin = TRUE;
    app_data.uploader_interval = 0;

    g_assert_false (rstrnt_log_manager_enabled (&app_data));

    app_data.stdin = TRUE;
    app_data.uploader_interval = 10;

    g_assert_false (rstrnt_log_manager_enabled (&app_data));
}

int
main (int    argc,
      char **argv)
{
    g_autoptr (RstrntLogManager) log_manager = NULL;
    SoupServer *server;
    g_autoptr (GError) error = NULL;
    g_autoptr (GSList) uris = NULL;
    RstrntTask *task;
    int retval;

    g_test_init (&argc, &argv, NULL);

    soup_session = soup_session_new ();

    log_manager = rstrnt_log_manager_get_instance ();
    server = soup_server_new (NULL, NULL);
    task = restraint_task_new ();

    soup_server_add_handler (server, "/" LOG_PATH_TASK, server_callback,
                             GINT_TO_POINTER (RSTRNT_LOG_TYPE_TASK), NULL);
    soup_server_add_handler (server, "/" LOG_PATH_HARNESS, server_callback,
                             GINT_TO_POINTER (RSTRNT_LOG_TYPE_HARNESS), NULL);

    g_test_add_data_func ("/logging/write", task, test_rstrnt_log_write);
    g_test_add_data_func ("/logging/upload", task, test_rstrnt_log_upload);
    g_test_add_func ("/logging/upload/no_logs", test_rstrnt_log_upload_no_logs);
    g_test_add_func ("/logging/chunking/zero_offset", test_rstrnt_chunk_log_zero_offset);
    g_test_add_func ("/logging/chunking/nonzero_offset", test_rstrnt_chunk_log_nonzero_offset);
    g_test_add_func ("/logging/enabled", test_rstrnt_log_manager_enabled);

    if (!soup_server_listen_local (server, 43770, SOUP_SERVER_LISTEN_IPV4_ONLY, &error))
    {
        g_error ("%s", error->message);
    }

    uris = soup_server_get_uris (server);

    task->task_id = g_strdup_printf ("%" G_GINT64_FORMAT, g_get_real_time ());
    task->task_uri = uris->data;

    retval = g_test_run ();

    soup_server_disconnect (server);
    g_object_unref (server);
    g_object_unref (soup_session);
    restraint_task_free (task);

    return retval;
}
