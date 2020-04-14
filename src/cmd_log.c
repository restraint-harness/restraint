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
#include <libsoup/soup.h>
#include "cmd_log.h"
#include "cmd_utils.h"
#include "errors.h"
#include "upload.h"
#include "utils.h"

void
format_log_server(ServerData *s_data, GError **error)
{
    if (s_data->server_recipe && s_data->task_id) {
        s_data->server = g_strdup_printf ("%s/tasks/%s", s_data->server_recipe, s_data->task_id);
    }
}

gboolean
parse_log_arguments(LogAppData *app_data, int argc, char *argv[], GError **error)
{

    gboolean ret = TRUE;

    GOptionEntry entries[] = {
        {"current", 'c', G_OPTION_FLAG_NONE, G_OPTION_FLAG_NONE,
            &app_data->s.curr_set, "Use current recipe/task id", NULL},
        {"pid", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT,
            &app_data->s.pid, "server pid", "PID"},
        {"server", 's', 0, G_OPTION_ARG_STRING, &app_data->s.server,
            "Server to connect to", "URL" },
        { "filename", 'l', 0, G_OPTION_ARG_STRING, &app_data->filename,
            "Log to upload", "FILE" },
        {"deprecated1", 'S', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &app_data->deprecated1,
            "deprecated option", NULL},
        {"deprecated2", 'T', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &app_data->deprecated2,
            "deprecated option", NULL},
        { NULL }
    };

    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Report results to lab controller. if you don't specify --current or the\n"
            "the server url you must have RECIPE_URL and TASKID defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPE_URL and TASKID");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, error);

    if (!parse_succeeded) {
        ret = FALSE;
        goto parse_cleanup;
    }

    if (!app_data->s.server) {
        format_server_string(&app_data->s, format_log_server, error);
        if (error != NULL && *error != NULL) {
            ret = FALSE;
            goto parse_cleanup;
        }
    }

    if (!app_data->filename || !app_data->s.server) {
        ret = FALSE;
        goto parse_cleanup;
    }

    if (app_data->deprecated1 != NULL) {
        g_warning ("Option -S deprecated! Update your code.\n");
    }
    if (app_data->deprecated2 != NULL) {
        g_warning ("Option -T deprecated! Update your code.\n");
    }

parse_cleanup:
    if (ret == FALSE) {
        cmd_usage(context);
    }
    g_option_context_free(context);
    return ret;
}

gboolean
upload_log (LogAppData *app_data, GError **error)
{
    SoupSession *session;
    SoupURI *result_uri = NULL;
    gchar *basename = NULL;
    gboolean ret = TRUE;

    result_uri = soup_uri_new (app_data->s.server);
    if (!result_uri) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", app_data->s.server);
        ret = FALSE;
        goto upload_cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);

    basename = g_filename_display_basename (app_data->filename);
    gchar *location = g_strdup_printf ("%s/logs/%s", app_data->s.server, basename);
    soup_uri_free(result_uri);
    result_uri = soup_uri_new (location);
    g_free (location);
    if (g_file_test (app_data->filename, G_FILE_TEST_EXISTS))
    {
        g_print ("Uploading %s ", basename);
        if (upload_file (session, app_data->filename, basename, result_uri, error)) {
            g_print ("done\n");
        } else {
            ret = FALSE;
            g_print ("failed\n");
        }
    }
    g_free(basename);
    soup_session_abort(session);
    g_object_unref(session);

upload_cleanup:
    if (result_uri != NULL) {
        soup_uri_free (result_uri);
    }
    return ret;

}
