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
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libsoup/soup.h>
#include "upload.h"

static SoupSession *session;

typedef struct {
    gchar *filename;
    gchar *outputfile;
    GPtrArray *disable_plugin;
} AppData;

static gboolean
callback_disable_plugin (const gchar *option_name, const gchar *value,
                 gpointer user_data, GError **error)
{
    AppData *app_data = (AppData *) user_data;

    g_ptr_array_add (app_data->disable_plugin, g_strdup (value));
    return TRUE;
}

static gboolean
callback_outputfile (const gchar *option_name, const gchar *value,
                     gpointer user_data, GError **error)
{
    AppData *app_data = (AppData *) user_data;

    app_data->filename = g_filename_display_basename (value);
    app_data->outputfile = g_strdup (value);
    return TRUE;
}

int main(int argc, char *argv[]) {

    AppData *app_data = g_slice_new0 (AppData);
    gchar filename[] = "logs/resultoutputfile.log";
    app_data->filename = filename;
    app_data->outputfile = getenv("OUTPUTFILE");
    app_data->disable_plugin = g_ptr_array_new_with_free_func (g_free);

    GError *error = NULL;

    gchar *server = NULL;
    SoupURI *result_uri;

    guint ret = 0;

    SoupMessage *server_msg;
    SoupRequest *request;

    gchar *result_msg = NULL;
    gchar *prefix = NULL;
    gchar *server_recipe_key = NULL;
    gchar *server_recipe = NULL;
    gchar *task_id = NULL;
    gchar *task_id_key = NULL;
    gboolean no_plugins = FALSE;

    gchar *form_data;
    GHashTable *data_table = g_hash_table_new (NULL, NULL);
    
    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { "message", 't', 0, G_OPTION_ARG_STRING, &result_msg,
            "Short 100 characters or less message", "TEXT" },
        { "outputfile", 'o', 0, G_OPTION_ARG_CALLBACK, callback_outputfile,
            "Log to upload with result, $OUTPUTFILE is used by default", "FILE" },
        { "disable-plugin", 'p', 0, G_OPTION_ARG_CALLBACK, callback_disable_plugin,
            "don't run plugin on server side", "PLUGIN" },
        { "no-plugins", 0, 0, G_OPTION_ARG_NONE, &no_plugins,
            "don't run any plugins on server side", NULL },
        { NULL }
    };
    GOptionGroup *option_group = g_option_group_new ("main",
                                                    "Application Options",
                                                    "Various application related options",
                                                    app_data, NULL);

    GOptionContext *context = g_option_context_new("TASK_PATH RESULT SCORE");
    g_option_context_set_summary(context,
            "Report results to lab controller. if you don't specify the\n"
            "the server url you must have RECIPEID and TASKID defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPEID and TASKID");
    g_option_group_add_entries(option_group, entries);
    g_option_context_set_main_group (context, option_group);

    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);

    if (!parse_succeeded) {
        goto cleanup;
    }

    prefix = getenv("HARNESS_PREFIX") ? getenv("HARNESS_PREFIX") : "";
    server_recipe_key = g_strdup_printf ("%sRECIPE_URL", prefix);
    server_recipe = getenv(server_recipe_key);
    task_id_key = g_strdup_printf ("%sTASKID", prefix);
    task_id = getenv(task_id_key);

    if (!server && server_recipe && task_id) {
        server = g_strdup_printf ("%s/tasks/%s/results/", server_recipe, task_id);
    }

    if (argc < 3 || !server) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    result_uri = soup_uri_new (server);
    session = soup_session_new_with_options("timeout", 3600, NULL);

    g_hash_table_insert (data_table, "path", argv[1]);
    g_hash_table_insert (data_table, "result", argv[2]);

    // if AVC_ERROR=+no_avc_check then disable the selinux check plugin
    // This is for legacy rhts tests.. please use --disable-plugin
    gchar *avc_error = getenv("AVC_ERROR");
    if (g_strcmp0 (avc_error, "+no_avc_check") == 0) {
        g_ptr_array_add (app_data->disable_plugin, g_strdup ("10_avc_check"));
    }

    if (app_data->disable_plugin->pdata) {
        g_hash_table_insert (data_table, "disable_plugin",
                             g_strjoinv (" ", (gchar **)app_data->disable_plugin->pdata));
    }
    g_ptr_array_free (app_data->disable_plugin, TRUE);
    if (no_plugins)
      g_hash_table_insert (data_table, "no_plugins", &no_plugins);
    if (argc > 3)
      g_hash_table_insert (data_table, "score", argv[3]);
    if (result_msg)
      g_hash_table_insert (data_table, "message", result_msg);

    request = (SoupRequest *)soup_session_request_http_uri (session, "POST", result_uri, &error);
    server_msg = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    soup_uri_free (result_uri);
    form_data = soup_form_encode_hash (data_table);
    soup_message_set_request (server_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));
    g_print ("** %s %s Score:%s\n", argv[1], argv[2], argv[3]);
    ret = soup_session_send_message (session, server_msg);
    if (SOUP_STATUS_IS_SUCCESSFUL (ret)) {
        gchar *location = g_strdup_printf ("%s/",
                                           soup_message_headers_get_one (server_msg->response_headers, "Location"));
        result_uri = soup_uri_new (location);
        g_free (location);
        if (g_file_test (app_data->outputfile, G_FILE_TEST_EXISTS)) 
        {
            g_print ("Uploading %s ", filename);
            if (upload_file (session, app_data->outputfile, app_data->filename, result_uri, &error)) {
                g_print ("done\n");
            } else {
                g_print ("failed\n");
            }
        }
        soup_uri_free (result_uri);
    } else {
       g_warning ("Failed to submit result, status: %d Message: %s\n", ret, server_msg->reason_phrase);
    }

cleanup:
    if (error) {
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        return error->code;
    } else {
        return EXIT_SUCCESS;
    }
}
