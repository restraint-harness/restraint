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

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "errors.h"
#include "utils.h"

void cmd_usage(GOptionContext *context) {
    gchar *usage_str = g_option_context_get_help(context, FALSE, NULL);
    g_print("%s", usage_str);
    g_free(usage_str);
}

guint64
parse_time_string(gchar *time_string, GError **error)
{
    /* Convert time string to number of seconds.
     *     5d -> 432000
     *     3m -> 180
     *     2h -> 7200
     *     600s -> 600
     */
    gchar time_unit;
    guint64 max_time = 0;
    gint read = sscanf(time_string, "%" G_GUINT64_FORMAT " %c", &max_time, &time_unit);
    if (read == 2) {
        time_unit = g_ascii_toupper(time_unit);
        if (time_unit == 'D')
            max_time = 24 * 3600 * max_time;
        else if (time_unit == 'H')
            max_time = 3600 * max_time;
        else if (time_unit == 'M')
            max_time = 60 * max_time;
        else if (time_unit == 'S')
            max_time = max_time;
        else {
            g_set_error (error, RESTRAINT_ERROR,
                         RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                         "Unrecognised time unit '%c'", time_unit);
        }
    } else if (read == 1) {
        max_time = max_time;
    } else {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to parse time string: %s", time_string);
    }
    return max_time;
}

gboolean
file_exists (gchar *filename)
{
    /* Checking if a file exists before creating it can be
     * prone to race conditions.  Be careful.
     */

    GStatBuf stat_buf;
    if (g_stat (filename, &stat_buf) == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

gchar *get_package_version(gchar *pkg_name, GError **error_out) {
    GError *gerror = NULL;
    gchar* std_out = NULL;
    gchar* std_err = NULL;
    gchar* err_out = NULL;
    gint exitstat = 0;

    gchar *command = g_strdup_printf("rpm -q --qf '%s' %s",
                                     "%{Version}",
                                     pkg_name);

    if (!g_spawn_command_line_sync(command, &std_out, &std_err,
                                   &exitstat, &gerror)) {
        g_set_error(error_out, RESTRAINT_ERROR,
                    RESTRAINT_CMDLINE_ERROR,
                    "Failed to spawn command: %s due to %s",
                    command, gerror->message);
    } else if (exitstat != 0) {
        if (std_err != NULL && (strlen(std_err) > 0)) {
            err_out = std_err;
        } else if (std_out != NULL && strlen(std_out) > 0) {
            err_out = std_out;
            std_out = NULL;
        }
        g_set_error(error_out, RESTRAINT_ERROR,
                    RESTRAINT_CMDLINE_ERROR,
                    "Get version command: %s, returned error: %s",
                    command, (err_out == NULL) ? "None" : err_out );
    }

    g_free(command);
    g_clear_error(&gerror);
    if (err_out != NULL) {
        g_free(err_out);
    } else if (std_err != NULL) {
        g_free(std_err);
    }

    return std_out;
}
