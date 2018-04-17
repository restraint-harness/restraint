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
#include "utils.h"
#include "errors.h"

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

    g_free(app_data->filename);
    app_data->filename = g_filename_display_basename (value);
    g_free(app_data->outputfile);
    app_data->outputfile = g_strdup (value);
    return TRUE;
}

static void restraint_free_appdata(AppData *app_data)
{
    if (app_data->filename != NULL) {
        g_free(app_data->filename);
    }
    if (app_data->outputfile != NULL) {
        g_free(app_data->outputfile);
    }
    g_ptr_array_free(app_data->disable_plugin, TRUE);
    g_slice_free(AppData, app_data);
}

int main(int argc, char *argv[]) {

    AppData *app_data = g_slice_new0 (AppData);
    app_data->filename = g_strdup("resultoutputfile.log");
    app_data->outputfile = g_strdup(getenv("OUTPUTFILE"));
    app_data->disable_plugin = g_ptr_array_new_with_free_func (g_free);

    GError *error = NULL;

    gchar *server = NULL;
    SoupURI *result_uri = NULL;

    guint ret = 0;

    SoupMessage *server_msg;
    SoupRequest *request;

    gchar *result_msg = NULL;
    gchar *prefix = NULL;
    gchar *server_recipe_key = NULL;
    gchar *server_recipe = NULL;
    gchar *task_id = NULL;
    gchar *task_id_key = NULL;
    gchar **positional_args = NULL;
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
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &positional_args,
            NULL, NULL},
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

    if (!parse_succeeded) {
        goto cleanup;
    }

    prefix = getenv("HARNESS_PREFIX") ? getenv("HARNESS_PREFIX") : "";
    server_recipe_key = g_strdup_printf ("%sRECIPE_URL", prefix);
    server_recipe = getenv(server_recipe_key);
    task_id_key = g_strdup_printf ("%sTASKID", prefix);
    task_id = getenv(task_id_key);
    g_free(task_id_key);
    g_free(server_recipe_key);

    if (!server && server_recipe && task_id) {
        server = g_strdup_printf ("%s/tasks/%s/results/", server_recipe, task_id);
    }

    if( positional_args == NULL ||
        server == NULL ||
        g_strv_length(positional_args) < 3 )
    {
        cmd_usage(context);
        goto cleanup;
    }

    result_uri = soup_uri_new (server);
    if (result_uri == NULL) {
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", server);
        goto cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);

    g_hash_table_insert (data_table, "path", positional_args[0]);
    g_hash_table_insert (data_table, "result", positional_args[1]);

    // if AVC_ERROR=+no_avc_check then disable the selinux check plugin
    // This is for legacy rhts tests.. please use --disable-plugin
    gchar *avc_error = getenv("AVC_ERROR");
    if (g_strcmp0 (avc_error, "+no_avc_check") == 0) {
        g_ptr_array_add (app_data->disable_plugin, g_strdup ("10_avc_check"));
    }

    if (app_data->disable_plugin->pdata) {
        g_ptr_array_add (app_data->disable_plugin, NULL);
        g_hash_table_insert (data_table, "disable_plugin",
                             g_strjoinv (" ", (gchar **)app_data->disable_plugin->pdata));
    }
    if (no_plugins)
      g_hash_table_insert (data_table, "no_plugins", &no_plugins);
    if (argc > 3)
      g_hash_table_insert (data_table, "score", positional_args[2]);
    if (result_msg)
      g_hash_table_insert (data_table, "message", result_msg);

    request = (SoupRequest *)soup_session_request_http_uri (session, "POST", result_uri, &error);
    server_msg = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    g_object_unref(request);
    form_data = soup_form_encode_hash (data_table);
    soup_message_set_request (server_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));
    g_print ("** %s %s Score:%s\n", argv[1], argv[2], argv[3]);
    ret = soup_session_send_message (session, server_msg);
    if (SOUP_STATUS_IS_SUCCESSFUL (ret)) {
        gchar *location = g_strdup_printf ("%s/logs/",
                                           soup_message_headers_get_one (server_msg->response_headers, "Location"));
        soup_uri_free (result_uri);
        result_uri = soup_uri_new (location);
        g_free (location);
        if (app_data->outputfile != NULL &&
            g_file_test (app_data->outputfile, G_FILE_TEST_EXISTS))
        {
            g_print ("Uploading %s ", app_data->filename);
            if (upload_file (session, app_data->outputfile, app_data->filename, result_uri, &error)) {
                g_print ("done\n");
            } else {
                g_print ("failed\n");
            }
        }
    } else {
       g_warning ("Failed to submit result, status: %d Message: %s\n", ret, server_msg->reason_phrase);
    }
    g_object_unref(server_msg);
    soup_session_abort(session);
    g_object_unref(session);

cleanup:
    if (server != NULL) {
        g_free(server);
    }
    if (result_msg != NULL) {
        g_free(result_msg);
    }
    g_option_context_free(context);
    g_hash_table_destroy(data_table);
    if (result_uri != NULL) {
        soup_uri_free (result_uri);
    }
    g_strfreev(positional_args);
    restraint_free_appdata(app_data);
    if (error) {
        int retcode = error->code;
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        g_clear_error(&error);
        return retcode;
    } else {
        return EXIT_SUCCESS;
    }
}
