#include "logging.c"

#include <glib.h>

static bool listening = true;

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

    path = g_build_path ("/",
                         VAR_LIB_PATH, "logs", task->task_id,
                         NULL);
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

    if (task_log_handled && harness_log_handled)
    {
        listening = false;

        soup_server_disconnect (server);
    }

    soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void
message_callback (SoupSession *session,
                  SoupMessage *msg,
                  gpointer     user_data)
{
    g_assert_cmpint (msg->status_code, ==, SOUP_STATUS_OK);
}

static void
queue_message (SoupSession           *session,
               SoupMessage           *msg,
               gpointer               msg_data,
               MessageFinishCallback  finish_callback,
               GCancellable          *cancellable,
               gpointer               user_data)
{
    soup_session_queue_message (session, msg, message_callback, NULL);
}

static void
test_rstrnt_log_upload (gconstpointer user_data)
{
    const RstrntTask *task;
    RstrntServerAppData app_data;
    SoupSession *session;

    task = user_data;
    session = soup_session_new ();

    app_data.queue_message = queue_message;

    rstrnt_upload_logs (task, &app_data, session, NULL);

    while (listening)
    {
        g_main_context_iteration (NULL, TRUE);
    }

    g_object_unref (session);
}

int
main (int    argc,
      char **argv)
{
    g_autoptr (RstrntLogManager) log_manager = NULL;
    SoupServer *server;
    g_autoptr (GError) error = NULL;
    g_autoptr (GSList) uris = NULL;
    RstrntTask task;
    int retval;

    g_test_init (&argc, &argv, NULL);

    log_manager = rstrnt_log_manager_get_instance ();
    server = soup_server_new (NULL, NULL);

    soup_server_add_handler (server, "/" LOG_PATH_TASK, server_callback,
                             GINT_TO_POINTER (RSTRNT_LOG_TYPE_TASK), NULL);
    soup_server_add_handler (server, "/" LOG_PATH_HARNESS, server_callback,
                             GINT_TO_POINTER (RSTRNT_LOG_TYPE_HARNESS), NULL);

    g_test_add_data_func ("/logging/write", &task, test_rstrnt_log_write);
    g_test_add_data_func ("/logging/upload", &task, test_rstrnt_log_upload);

    if (!soup_server_listen_local (server, 43770, SOUP_SERVER_LISTEN_IPV4_ONLY, &error))
    {
        g_error ("%s", error->message);
    }

    uris = soup_server_get_uris (server);

    task.task_id = "1997";
    task.task_uri = uris->data;

    retval = g_test_run ();

    g_object_unref (server);

    return retval;
}
