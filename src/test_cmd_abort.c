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

#include "cmd_abort.h"
#include "cmd_utils.h"
#include "errors.h"
#include "utils.h"

#define CMD_RSTRNT "rstrnt-abort"

/* test restraint abort cmd with 'server' arg*/
#define SARG_SERVER_STRING "http://localhost:46344/recipes/1/status"
static void
test_rstrnt_abort_server()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "-s",
        SARG_SERVER_STRING
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(SARG_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);
}

/* test restraint abort cmd with short 'c&i' args */
#define SHORTARGS_SERVER_STRING "http://localhost:46344/recipes/1/status"
static void
test_rstrnt_abort_short_c_and_i()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *mypid = g_strdup_printf("%d", getpid());

    char *argv[] = {
        CMD_RSTRNT,
        "-c",
        "-i",
        mypid  // need to use our own pid since we created the file
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", "1",
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(SHORTARGS_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);
    g_free(mypid);
}

/* test restraint abort cmd with long 'current&pid' args */
#define LONGARGS_SERVER_STRING SHORTARGS_SERVER_STRING
static void
test_rstrnt_abort_long_current_and_pid()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *mypid = g_strdup_printf("%d", getpid());

    char *argv[] = {
        CMD_RSTRNT,
        "--current",
        "--pid",
        mypid    // need to use our own pid since we created the file
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", "1",
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(LONGARGS_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);
    g_free(mypid);
}


/* test restraint abort cmd with long 'current&pid' args and
 * -t type 'task' */
#define TYPE_TASK_SERVER_STRING "http://localhost:46344/recipes/1/tasks/999/status"
static void
test_rstrnt_abort_long_current_and_pid_and_type_task()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *mypid = g_strdup_printf("%d", getpid());

    char *argv[] = {
        CMD_RSTRNT,
        "--current",
        "--pid",
        mypid,   // need to use our own pid since we created the file
        "--type",
        "task"
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", "999",
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(TYPE_TASK_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);
    g_free(mypid);
}


/* test restraint abort cmd -t type 'task' missing task-id error. */
static void
test_rstrnt_abort_type_task_missing_taskid_error()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *mypid = g_strdup_printf("%d", getpid());

    char *argv[] = {
        CMD_RSTRNT,
        "--current",
        "--pid",
        mypid,   // need to use our own pid since we created the file
        "--type",
        "task"
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", NULL,
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_false(rc);
    g_assert_error(error, RESTRAINT_ERROR, RESTRAINT_CMDLINE_ERROR);

    if (error) {
        g_assert_cmpstr(error->message, ==,
                        "Requested type task-abort but no task_id available. Quitting.\n");
        g_clear_error(&error);
    }
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);
    g_free(mypid);
}

/* test restraint abort cmd using environment vars and not args*/
#define ENV_SERVER_STRING "http://localhost:99999/recipes/2/status"
static void
test_environment_variables()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *argv[] = {
        CMD_RSTRNT
    };
    int argc = sizeof(argv) / sizeof(char*);
    set_envvar_from_file(99999, &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_true(rc);
    g_assert_cmpstr(ENV_SERVER_STRING, ==, app_data->s.server);

    if (error) {
        g_clear_error(&error);
    }
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);

}
/* test restraint abort cmd env_file not exist */
static void
test_rstrnt_abort_current_file_not_exist()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "-c",
        "-i",
        "11111"   /* does not exist. */
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", "1",
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
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
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);

}


/* test restraint abort cmd Error Case Failed to get restraintd pid */
static void
test_rstrnt_abort_no_restraintd_running()
{
    GError *error = NULL;
    AbortAppData *app_data = g_slice_new0 (AbortAppData);
    char *argv[] = {
        CMD_RSTRNT,
        "-c"
    };
    int argc = sizeof(argv) / sizeof(char*);

    // Environment file to be used by --current option
    update_env_script("RSTRNT_", "http://localhost:46344", "1", "1",
                      &error);
    g_assert_no_error(error);

    gboolean rc = parse_abort_arguments(app_data, argc, argv, &error);
    g_assert_false(rc);
    g_assert_error(error, RESTRAINT_ERROR,
                   RESTRAINT_NO_RESTRAINTD_RUNNING_ERROR);

    if (error) {
        int rc = g_strcmp0(error->message,
                           "Failed to get restraintd pid. Server not running.");
        g_assert_cmpint(rc, ==, 0);
        g_clear_error(&error);
    }
    unset_envvar_from_file(getpid(), &error);
    clear_server_data(&app_data->s);
    g_slice_free(AbortAppData, app_data);

}


int main(int argc, char *argv[])
{
    GError *error = NULL;

    g_test_init(&argc, &argv, NULL);
    gchar *oldfilename = get_envvar_filename(getpid());
    gchar *newfilename = get_envvar_filename(99999);

    // Environment file to be used by environmnet variables
    update_env_script("RSTRNT_", "http://localhost:99999", "2", "2",
                      &error);
    g_assert_no_error(error);
    g_rename(oldfilename, newfilename);
    g_free(oldfilename);
    g_free(newfilename);

    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_server",
                    test_rstrnt_abort_server);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_short_c_and_i",
                    test_rstrnt_abort_short_c_and_i);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_long_current_and_pid",
                    test_rstrnt_abort_long_current_and_pid);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_long_current_and_pid_and_type_task",
                    test_rstrnt_abort_long_current_and_pid_and_type_task);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_type_task_missing_taskid_error",
                    test_rstrnt_abort_type_task_missing_taskid_error);
    g_test_add_func("/cmd_upload/rhts_test_environment_variables",
                    test_environment_variables);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_current_file_not_exist",
                    test_rstrnt_abort_current_file_not_exist);
    g_test_add_func("/cmd_upload/rhts_test_rstrnt_abort_no_restraintd_running",
                    test_rstrnt_abort_no_restraintd_running);


    return g_test_run();
}
