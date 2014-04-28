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

static SoupSession *session;

int main(int argc, char *argv[]) {

    GError *error = NULL;

    gchar *server = NULL;
    gchar *form_data;
    SoupURI *watchdog_uri;
    guint ret = 0;

    gchar *prefix = NULL;
    gchar *recipe_id_key = NULL;
    gchar *recipe_id = NULL;
    GHashTable *data_table = g_hash_table_new (NULL, NULL);

    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { NULL }
    };
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Adjust watchdog on lab controller. if you don't specify the\n"
            "the server url you must have RECIPEID defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPEID");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);

    if (!parse_succeeded) {
        goto cleanup;
    }

    guint64 seconds = parse_time_string (argv[1], &error);
    if (error) {
        goto cleanup;
     }

    prefix = getenv("HARNESS_PREFIX") ? getenv("HARNESS_PREFIX") : "";
    recipe_id_key = g_strdup_printf ("%sRECIPEID", prefix);
    recipe_id = getenv(recipe_id_key);

    if (!server && recipe_id) {
        server = g_strdup_printf ("http://localhost:8081/recipes/%s/watchdog", recipe_id);
    }

    if (!server) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    watchdog_uri = soup_uri_new (server);
    session = soup_session_new_with_options("timeout", 3600, NULL);
    SoupMessage *server_msg = soup_message_new_from_uri ("POST", watchdog_uri);
    g_hash_table_insert (data_table, "seconds", &seconds);
    form_data = soup_form_encode_hash (data_table);
    soup_message_set_request (server_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));

    ret = soup_session_send_message (session, server_msg);
    if (SOUP_STATUS_IS_SUCCESSFUL (ret)) {
    } else {
        g_warning ("Failed to adjust watchdog, status: %d Message: %s\n", ret, server_msg->reason_phrase);
    }

    soup_uri_free (watchdog_uri);

cleanup:
    if (error) {
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        return error->code;
    } else {
        return EXIT_SUCCESS;
    }
}
