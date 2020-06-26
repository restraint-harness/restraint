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
 * update_env_file()
 *
 * The environment variables RECIPE_URL and TASKID need
 * to be put somewhere so the user can conveniently
 * run restraintd commands.
 * If user wants to run commands in this case, they should first do:
 *    export $(cat /etc/profile.d/rstrnt-commands-env.sh)
 * to acquire the current environment variables.
 */
void update_env_file (gchar *prefix, gchar *restraint_url,
                      gchar *recipe_id, gchar *task_id,
                      guint port, GError **error)
{
    gchar *filename = get_envvar_filename(port);
    FILE *env_file;

    env_file = g_fopen(filename, "w");

    g_free(filename);
    if (env_file == NULL) {
        g_set_error (error, RESTRAINT_ERROR, RESTRAINT_OPEN,
                     "update_env_file env_file is NULL, errno=%s\n",
                     g_strerror (errno));
        return;
    }

    g_fprintf(env_file, "HARNESS_PREFIX=%s\n", prefix);
    g_fprintf(env_file, "%sURL=%s\n", prefix, restraint_url);
    g_fprintf(env_file, "%sRECIPE_URL=%s/recipes/%s\n", prefix, restraint_url, recipe_id);
    g_fprintf(env_file, "%sTASKID=%s\n", prefix, task_id);
    fclose(env_file);
}

void
remove_env_file(guint port)
{
    gchar *filename = get_envvar_filename(port);
    g_remove(filename);
    g_free(filename);
}

/* Convert time string to number of seconds.
 *     5d -> 432000
 *     3m -> 180
 *     2h -> 7200
 *     600s -> 600
 *
 * If parsing fails, error is set and 0 is returned.
 */
guint64
parse_time_string (gchar *time_string, GError **error)
{
    gchar   time_unit;
    gint    read;
    guint64 time_value = 0;

    g_return_val_if_fail (time_string != NULL && strlen (time_string) > 0, 0);
    g_return_val_if_fail (error == NULL || *error == NULL, 0);

    errno = 0;

    read = sscanf (time_string, "%" G_GUINT64_FORMAT " %c", &time_value, &time_unit);

    if (read == 0 || errno != 0) {
        g_set_error (error,
                     RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to parse time string: %s",
                     time_string);

        return 0;
    }

    time_unit = read == 1 ? 'S' : g_ascii_toupper (time_unit);

    switch (time_unit) {
    case 'D':
        return time_value * 24 * 60 * 60;
    case 'H':
        return time_value * 60 * 60;
    case 'M':
        return time_value * 60;
    case 'S':
        return time_value;
    default:
        g_set_error (error,
                     RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Unrecognised time unit '%c'",
                     time_unit);
        break;
    }

    return 0;
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
        gchar  *err_out;

        if (std_out != NULL && strlen (std_out) > 0) {
            err_out = std_out;
        } else if (std_err != NULL && strlen (std_err) > 0) {
            err_out = std_err;
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
get_envvar_filename(guint port)
{
    gboolean result;
    gchar *filename;

    /* If the expected directory is present, this is an actual installation */
    result = g_file_test(CMD_ENV_DIR, G_FILE_TEST_IS_DIR);
    if (result) {
        filename = g_strdup_printf(CMD_ENV_FILE_FORMAT, CMD_ENV_DIR, port);
    } else {
        filename = g_strdup_printf(CMD_ENV_FILE_FORMAT, "./", port);
    }
    return(filename);
}

/* get_install_dir()
 *
 * Get directory to install tasks by configured file or default.
 *
 * If there was an unexpected result in getting data from
 * configured install_config file, an error will be returned
 * with default_install_path.
 */
gchar *
get_install_dir(const gchar *filename, GError **error)
{
    GKeyFile *keyfile = NULL;
    gchar *install_dir_value;

    g_return_val_if_fail(filename != NULL, NULL);

    if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
        keyfile = g_key_file_new();
        /* Load the GKeyFile from install_dir. */
        if (g_key_file_load_from_file(keyfile, filename,
                                      G_KEY_FILE_NONE, error)) {
            install_dir_value = g_key_file_get_string(keyfile,
                                                      "General",
                                                      INSTALL_DIR_VAR,
                                                      error);
            if (install_dir_value)  {
                if (strlen(install_dir_value) == 0) {
                    free(install_dir_value);
                    g_set_error (error, RESTRAINT_ERROR,
                                 RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                                 "No value set in var %s file %s",
                                 INSTALL_DIR_VAR, filename);
                } else {
                    goto get_install_dir_exit;
                }
            }
        }
    }

    // If install_config file doesn't exist or there was a failure
    // getting data from it, provide default and error.

    install_dir_value = g_strdup_printf(INSTALL_DIR_DEFAULT);

get_install_dir_exit:
    if (keyfile != NULL) {
        g_key_file_free(keyfile);
    }
    return install_dir_value;
}
