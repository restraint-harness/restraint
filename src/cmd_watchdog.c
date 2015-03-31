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
#include "utils.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "errors.h"

static SoupSession *session;

int main(int argc, char *argv[]) {

    GError *error = NULL;

    gchar *server = NULL;
    gchar *form_data;
    gchar *form_seconds;
    SoupURI *watchdog_uri = NULL;
    guint ret = 0;
    guint64 seconds;

    gchar *prefix = NULL;
    gchar *server_recipe_key = NULL;
    gchar *server_recipe = NULL;
    GHashTable *data_table = g_hash_table_new (NULL, NULL);

    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { NULL }
    };
    GOptionContext *context = g_option_context_new("<time>");
    g_option_context_set_summary(context,
            "Adjust watchdog on lab controller. if you don't specify the\n"
            "the server url you must have RECIPEID defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPEID");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);

    if (argc < 2 || !parse_succeeded) {
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Wrong arguments");
        cmd_usage(context);
        goto cleanup;
    }

    seconds = parse_time_string (argv[1], &error);
    if (error) {
        cmd_usage(context);
        goto cleanup;
     }

    prefix = getenv("HARNESS_PREFIX") ? getenv("HARNESS_PREFIX") : "";
    server_recipe_key = g_strdup_printf ("%sRECIPE_URL", prefix);
    server_recipe = getenv(server_recipe_key);
    g_free(server_recipe_key);

    if (!server && server_recipe) {
        server = g_strdup_printf ("%s/watchdog", server_recipe);
    }

    if (!server) {
        cmd_usage(context);
        goto cleanup;
    }

    watchdog_uri = soup_uri_new (server);
    if (!watchdog_uri) {
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", server);
        goto cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);
    SoupMessage *server_msg = soup_message_new_from_uri ("POST", watchdog_uri);
    form_seconds = g_strdup_printf ("%" G_GUINT64_FORMAT, seconds);
    g_hash_table_insert (data_table, "seconds", form_seconds);
    form_data = soup_form_encode_hash (data_table);
    g_free (form_seconds);
    soup_message_set_request (server_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));

    ret = soup_session_send_message (session, server_msg);
    if (SOUP_STATUS_IS_SUCCESSFUL (ret)) {
    } else {
        g_warning ("Failed to adjust watchdog, status: %d Message: %s\n", ret, server_msg->reason_phrase);
    }
    g_object_unref(server_msg);

    soup_session_abort(session);
    g_object_unref(session);

cleanup:
    g_hash_table_destroy(data_table);
    g_option_context_free(context);
    if (server != NULL) {
        g_free(server);
    }
    if (watchdog_uri != NULL) {
        soup_uri_free (watchdog_uri);
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
