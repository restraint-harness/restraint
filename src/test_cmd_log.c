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
#include <glib/gstdio.h>
#include <archive.h>
#include <string.h>
#include <unistd.h>

#include "cmd_log.h"
#include "cmd_utils.h"
#include "errors.h"
#include "utils.h"

#define CMD_RSTRNT "rstrnt-result-log"

/* test restraint log cmd with 'server' arg*/
#define SARG_SERVER_STRING "http://localhost:46344/recipes/1/tasks/1"
static void
test_rstrnt_log_server()
{
    GError *error = NULL;
    LogAppData *app_data = g_slice_new0 (LogAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "-s",
        SARG_SERVER_STRING,
        "--filename",
        "LogFile"
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_log_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(SARG_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    g_free(app_data->filename);
    clear_server_data(&app_data->s);
    g_slice_free(LogAppData, app_data);
}

/* test restraint log cmd with short 'c&i' args */
#define SHORTARGS_SERVER_STRING "http://localhost:46344/recipes/1/tasks/1"
static void
test_rstrnt_log_port()
{
    GError *error = NULL;
    LogAppData *app_data = g_slice_new0 (LogAppData);
    guint port = 46344;

    char *argv[] = {
        CMD_RSTRNT,
        "--port",
        "46344",
        "-l",
        "LogFile"
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_file("RSTRNT_", "http://localhost:46344", "1", "1",
                    port, &error);
    g_assert_no_error(error);

    gboolean rc = parse_log_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(SHORTARGS_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    unset_envvar_from_file(port, &error);
    g_free(app_data->filename);
    clear_server_data(&app_data->s);
    g_slice_free(LogAppData, app_data);
    remove_env_file(port);
}

/* test restraint log cmd using environment vars and not args*/
#define ENV_SERVER_STRING "http://localhost:99999/recipes/2/tasks/2"
static void
test_log_environment_variables()
{
    GError *error = NULL;
    LogAppData *app_data = g_slice_new0 (LogAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "-l",
        "LogFile"
    };
    int argc = sizeof(argv) / sizeof(char*);
    guint port = 99999;

    // Environment file to be used by environmnet variables
    update_env_file("RSTRNT_", "http://localhost:99999", "2", "2",
                    port, &error);
    g_assert_no_error(error);
    set_envvar_from_file(port, &error);
    g_assert_no_error(error);

    gboolean rc = parse_log_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(ENV_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    if (app_data->filename) {
        g_free(app_data->filename);
    }
    unset_envvar_from_file(port, &error);
    clear_server_data(&app_data->s);
    g_slice_free(LogAppData, app_data);
    remove_env_file(port);
}

/* test restraint log cmd using environment vars and not args*/
static void
test_log_port_file_not_exist()
{
    GError *error = NULL;
    LogAppData *app_data = g_slice_new0 (LogAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "--port",
        "11111",   /* does not exist. */
        "-l",
        "LogFile"
    };
    guint port = 46344;
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_file("RSTRNT_", "http://localhost:46344", "1", "1",
                    port, &error);
    g_assert_no_error(error);

    gboolean rc = parse_log_arguments(app_data, argc, argv, &error);
    g_assert_false(rc);
    g_assert_error(error, RESTRAINT_ERROR, RESTRAINT_MISSING_FILE);

    if (error) {
        gchar *filename = get_envvar_filename(11111);
        gchar *expect_msg = g_strdup_printf("File %s not present",
                                            filename);
        int rc = g_strcmp0(error->message, expect_msg);
        g_free(expect_msg);
        g_free(filename);
        g_assert_cmpint(rc, ==, 0);
        g_clear_error(&error);
    }
    unset_envvar_from_file(port, &error);
    if (app_data->filename) {
        g_free(app_data->filename);
    }
    clear_server_data(&app_data->s);
    g_slice_free(LogAppData, app_data);
    remove_env_file(port);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/cmd_upload/rhts_test_rstrnt_log_server",
                    test_rstrnt_log_server);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_log_port",
                    test_rstrnt_log_port);
    g_test_add_func("/cmd_upload/rhts_test_log_environment_variables",
                    test_log_environment_variables);
    g_test_add_func("/cmd_upload/rhts_test_log_port_file_not_exist",
                    test_log_port_file_not_exist);

    return g_test_run();
}
