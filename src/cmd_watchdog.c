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
#include <curl/curl.h>
#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "cmd_watchdog.h"
#include "cmd_utils.h"
#include "errors.h"
#include "process.h"
#include "utils.h"

void
format_watchdog_server(ServerData *s_data)
{
    if (s_data->server_recipe) {
        s_data->server = g_strdup_printf ("%s/watchdog", s_data->server_recipe);
    }
}


gboolean
parse_watchdog_arguments(WatchdogAppData *app_data, int argc, char *argv[], GError **error) {

    gboolean ret = TRUE;


    GOptionEntry entries[] = {
        {"port", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_INT,
            &app_data->s.port, "restraintd port number (Service default: 8081)", "PORT"},
        {"server", 's', 0, G_OPTION_ARG_STRING, &app_data->s.server,
            "Server to connect to", "URL" },
        { NULL }
    };

    GOptionContext *context = g_option_context_new("<time>");
    g_option_context_set_summary(context,
            "Adjust watchdog on lab controller. if you don't specify --port or \n"
            "the server url you must have RECIPE_URL defined.\n"
            "If HARNESS_PREFIX is defined then the value of that must be\n"
            "prefixed to RECIPE_URL");
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, error)) {
        ret = FALSE;
        goto parse_cleanup;
    }

    if (argc < 2) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Wrong arguments");
        ret = FALSE;
        goto parse_cleanup;
    }

    app_data->seconds = parse_time_string (argv[1], error);
    if (*error) {
        ret = FALSE;
        goto parse_cleanup;
     }

    if (!app_data->s.server) {
        format_server_string(&app_data->s, format_watchdog_server, error);
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
    g_option_context_free(context);
    return ret;
}

gboolean
upload_watchdog (WatchdogAppData *app_data, GError **error)
{
    CURL *curl;
    CURLcode res;
    long response_code;
    char *form_data = NULL;
    char *form_seconds = NULL;
    gboolean result = TRUE;
    struct curl_slist *headers = NULL;

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to initialize curl");
        result = FALSE;
        goto cleanup;
    }

    /* Prepare form data */
    form_seconds = g_strdup_printf ("%" G_GUINT64_FORMAT, app_data->seconds);
    form_data = g_strdup_printf("seconds=%s", form_seconds);

    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, app_data->s.server);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(form_data));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "restraint-client");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    
    /* Set content type header */
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    if (!headers) {
	goto cleanup;
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Perform the request */
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to adjust watchdog: %s", curl_easy_strerror(res));
        result = FALSE;
        goto cleanup;
    }

    /* Check HTTP response code */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 200 && response_code < 300) {
        if (app_data->seconds < HEARTBEAT) {
            g_warning ("Expect up to a 1 minute delay for watchdog thread to notice change.\n");
        }
    } else {
        result = FALSE;
        g_warning ("Failed to adjust watchdog, HTTP status: %ld\n", response_code);
    }

cleanup:
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (form_data) {
        g_free(form_data);
    }
    if (form_seconds) {
        g_free(form_seconds);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }

    return result;
}
