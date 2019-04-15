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

int main(int argc, char *argv[]) {

    GError *error = NULL;

    gchar *server = NULL;
    SoupURI *result_uri = NULL;

    gchar *filename = NULL;
    gchar *basename = NULL;
    gchar *server_recipe = NULL;
    gchar *task_id = NULL;

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

    if (!parse_succeeded) {
        goto cleanup;
    }

    server_recipe = get_recipe_url();
    task_id = get_taskid();

    if (!server && server_recipe && task_id) {
        server = g_strdup_printf ("%s/tasks/%s", server_recipe, task_id);
    }

    if (!filename || !server) {
        cmd_usage(context);
        goto cleanup;
    }

    if (deprecated1 != NULL) {
        g_warning ("Option -S deprecated! Update your code.\n");
    }
    if (deprecated2 != NULL) {
        g_warning ("Option -T deprecated! Update your code.\n");
    }

    result_uri = soup_uri_new (server);
    if (!result_uri) {
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", server);
        goto cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);

    basename = g_filename_display_basename (filename);
    gchar *location = g_strdup_printf ("%s/logs/%s", server, basename);
    soup_uri_free(result_uri);
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
    g_free(basename);
    soup_session_abort(session);
    g_object_unref(session);

cleanup:
    g_option_context_free(context);
    if (result_uri != NULL) {
        soup_uri_free (result_uri);
    }
    if (filename != NULL) {
        g_free(filename);
    }
    if (server != NULL) {
        g_free(server);
    }
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
