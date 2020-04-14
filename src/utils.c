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
#include <unistd.h>
#include "errors.h"
#include "utils.h"

/*
 * update_env_script()
 *
 * The restraint TCP port no longer static,
 * the environment variables RECIPE_URL and TASKID need
 * to be put somewhere so the user can conveniently
 * run restraintd commands.
 * If user wants to run commands in this case, they should first do:
 *    export $(cat /etc/profile.d/rstrnt-commands-env.sh)
 * to acquire the current environment variables.
 */
void update_env_script(gchar *prefix, gchar *restraint_url,
                       gchar *recipe_id, gchar *task_id,
                       GError **error)
{
    gchar *filename = get_envvar_filename(getpid());
    FILE *env_file;

    env_file = g_fopen(filename, "w");

    g_free(filename);
    if (env_file == NULL) {
        g_set_error (error, RESTRAINT_ERROR, RESTRAINT_OPEN,
                     "update_env_script env_file is NULL, errno=%s\n",
                     g_strerror (errno));
        return;
    }

    g_fprintf(env_file, "HARNESS_PREFIX=%s\n", prefix);
    g_fprintf(env_file, "%sURL=%s\n", prefix, restraint_url);
    g_fprintf(env_file, "%sRECIPE_URL=%s/recipes/%s\n", prefix, restraint_url, recipe_id);
    if (task_id) {
        g_fprintf(env_file, "%sTASKID=%s\n", prefix, task_id);
    }
    fclose(env_file);
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

/*
 * Returns the rpm package version or NULL if there is any error.
 */
gchar *
get_package_version (gchar   *pkg_name,
                     GError **error_out)
{
    GError *gerror = NULL;
    gchar  *command = NULL;
    gchar  *err_out = NULL;
    gchar  *std_err = NULL;
    gchar  *std_out = NULL;
    gint    exitstat;

    command = g_strdup_printf ("rpm -q --qf '%s' %s",
                               "%{Version}",
                               pkg_name);

    if (!g_spawn_command_line_sync (command, &std_out, &std_err, &exitstat, &gerror)) {
        g_set_error (error_out, RESTRAINT_ERROR, RESTRAINT_CMDLINE_ERROR,
                     "Failed to spawn command: %s due to %s",
                     command, gerror->message);
    } else if (exitstat != 0) {
        if (std_err != NULL && strlen (std_err) > 0) {
            err_out = std_err;
        } else if (std_out != NULL && strlen (std_out) > 0) {
            err_out = std_out;
        } else {
            err_out = "None";
        }

        g_set_error (error_out, RESTRAINT_ERROR, RESTRAINT_CMDLINE_ERROR,
                     "Get version command: %s, returned error: %s",
                     command, err_out);

        g_free (std_out);
        std_out = NULL;
    }

    g_clear_error (&gerror);
    g_free (command);
    g_free (std_err);

    return std_out;
}

/* get_envvar_filename()
 *
 * Retries the correct name which can vary depending
 * on authentic installation or running UTs with gtester.
 */
gchar *
get_envvar_filename(gint pid)
{
    gboolean result;
    gchar *filename;

    /* If the expected directory is present, this is an actual installation */
    result = g_file_test(CMD_ENV_DIR, G_FILE_TEST_IS_DIR);
    if (result) {
        filename = g_strdup_printf(CMD_ENV_FILE_FORMAT, CMD_ENV_DIR, pid);
    } else {
        filename = g_strdup_printf(CMD_ENV_FILE_FORMAT, "./", pid);
    }
    return(filename);
}
