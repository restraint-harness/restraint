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

#include "cmd_utils.h"
#include "cmd_abort.h"
#include "errors.h"

void
format_recipe_abort_server(ServerData *s_data, GError **error)
{
    if (s_data->server_recipe) {
        s_data->server = g_strdup_printf ("%s/status", s_data->server_recipe);
    }
}
void
format_task_abort_server(ServerData *s_data, GError **error)
{
    if (s_data->task_id == NULL) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_CMDLINE_ERROR,
                     "Requested type task-abort but no task_id available. Quitting.\n");
        return;
    }

    if (s_data->server_recipe) {
        s_data->server = g_strdup_printf ("%s/tasks/%s/status",
                                          s_data->server_recipe, s_data->task_id);
    }
}
gboolean
parse_abort_arguments(AbortAppData *app_data, int argc, char *argv[], GError **error)
{
    gchar *type = NULL;
    gboolean ret = TRUE;

    GOptionEntry entries[] = {
        {"current", 'c', G_OPTION_FLAG_NONE, G_OPTION_FLAG_NONE,
            &app_data->s.curr_set, "Use current recipe/task id", NULL},
        {"pid", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT,
            &app_data->s.pid, "server pid", "PID"},
        {"server", 's', 0, G_OPTION_ARG_STRING, &app_data->s.server,
            "Server to connect to", "URL" },
        {"type", 't', 0, G_OPTION_ARG_STRING, &type,
            "Specify abort type. Dflt: recipe", "<task|recipe>"},
        { NULL }
    };

    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Aborts currently running recipe or task. if you don't specify --current or \n"
            "the server url you must have RECIPE_URL defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPE_URL");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, error);

    if (!parse_succeeded) {
        ret = FALSE;
        goto parse_cleanup;
    }

    if (!app_data->s.server) {
        if (g_strcmp0(type, "task") == 0) {
            format_server_string(&app_data->s, format_task_abort_server, error);
        } else {
            format_server_string(&app_data->s, format_recipe_abort_server, error);
        }
        if (error != NULL && *error != NULL) {
            ret = FALSE;
            goto parse_cleanup;
        }
    }

    if (!app_data->s.server) {
        ret = FALSE;
        goto parse_cleanup;
    }

parse_cleanup:
    if (ret == FALSE) {
        cmd_usage(context);
    }
    free(type);
    g_option_context_free(context);
    return ret;
}

gboolean
upload_abort (AbortAppData *app_data, GError **error)
{
    gboolean result = TRUE;
    guint ret = 0;
    SoupSession *session;

    SoupURI *server_uri = NULL;

    server_uri = soup_uri_new(app_data->s.server);
    if (!server_uri) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", app_data->s.server);
        result = FALSE;
        goto upload_cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);
    SoupMessage *msg = soup_message_new_from_uri("POST", server_uri);
    char *form = soup_form_encode("status", "Aborted", NULL);
    soup_message_set_request (msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form, strlen (form));
    ret = soup_session_send_message(session, msg);
    if (!SOUP_STATUS_IS_SUCCESSFUL(ret)) {
        g_warning ("Failed to abort job, status: %d Message: %s\n", ret,
                   msg->reason_phrase);
        result = FALSE;
    }

    g_object_unref(msg);
    soup_session_abort(session);
    g_object_unref(session);

upload_cleanup:

    if (server_uri != NULL) {
        soup_uri_free (server_uri);
    }

    return (result);
}
