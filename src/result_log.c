#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libsoup/soup.h>
#include "common.h"
#include "upload.h"

static SoupSession *session;

int main(int argc, char *argv[]) {

    GError *error = NULL;

    gchar *server = NULL;
    SoupURI *result_uri;

    gchar *filename = NULL;
    gchar *basename = NULL;
    gchar *prefix = NULL;
    gchar *recipe_id_key = NULL;
    gchar *recipe_id = NULL;
    gchar *task_id = NULL;
    gchar *task_id_key = NULL;

    gchar *deprecated1 = NULL;
    gchar *deprecated2 = NULL;

    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { "filename", 'l', 0, G_OPTION_ARG_STRING, &filename,
            "Log to upload", "FILE" },
        {"deprecated1", 'S', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &deprecated1,
            "deprecated option", NULL},
        {"deprecated2", 'T', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &deprecated2,
            "deprecated option", NULL},
        { NULL }
    };
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Report results to lab controller. if you don't specify the\n"
            "the server url you must have RECIPEID and TASKID defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPEID and TASKID");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);

    if (!parse_succeeded) {
        goto cleanup;
    }

    prefix = getenv("HARNESS_PREFIX") ? getenv("HARNESS_PREFIX") : "";
    recipe_id_key = g_strdup_printf ("%sRECIPEID", prefix);
    recipe_id = getenv(recipe_id_key);
    task_id_key = g_strdup_printf ("%sTASKID", prefix);
    task_id = getenv(task_id_key);

    if (!server && recipe_id && task_id) {
        server = g_strdup_printf ("http://localhost:8081/recipes/%s/tasks/%s", recipe_id, task_id);
    }

    if (!filename || !server) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    if (deprecated1 != NULL) {
        g_warning ("Option -S deprecated! Update your code.\n");
    }
    if (deprecated2 != NULL) {
        g_warning ("Option -T deprecated! Update your code.\n");
    }

    result_uri = soup_uri_new (server);
    session = soup_session_new_with_options("timeout", 3600, NULL);

    basename = g_filename_display_basename (filename);
    gchar *location = g_strdup_printf ("%s/logs/%s", server, basename);
    result_uri = soup_uri_new (location);
    g_free (location);
    if (g_file_test (filename, G_FILE_TEST_EXISTS)) 
    {
        g_print ("Uploading %s ", basename);
        if (upload_file (session, filename, basename, result_uri, &error)) {
            g_print ("done\n");
        } else {
            g_print ("failed\n");
        }
    }
    soup_uri_free (result_uri);

cleanup:
    if (error) {
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        return error->code;
    } else {
        return EXIT_SUCCESS;
    }
}
