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

#include "cmd_result.h"

#define CMD_RSTRNT "rstrnt-report-result"
#define CMD_RHTS RHTS_COMPAT_FILENAME

/* Environment variables that rstrnt-report-result uses */
#define OUTPUTFILE "OUTPUTFILE"
#define RECIPE_URL "RECIPE_URL"
#define TASK_ID "TASKID"
#define HARNESS_PREFIX "HARNESS_PREFIX"

/* check that the 3arg form of rhts parses correctly */
static void
test_rhts_3arg()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "my_awesome_test";
    char *test_result = "SUPER GOOD";
    char *logfile = "my_logfile";

    char *recipe_url = "http://localhost";
    char *task_id = "foobar";

    g_setenv(RECIPE_URL, recipe_url, TRUE);
    g_setenv(TASK_ID, task_id, TRUE);

    char *argv[] = {
        CMD_RHTS,
        test_name,
        test_result,
        logfile
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_true(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->outputfile, ==, logfile);
    g_assert_null(app_data->score);

    g_unsetenv(RECIPE_URL);
    g_unsetenv(TASK_ID);

    restraint_free_appdata(app_data);
}

/* Check that 4 positional args are parsed properly */
static void
test_rhts_4arg()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "my_awesome_test";
    char *test_result = "SUPER GOOD";
    char *logfile = "my_logfile";
    char *score = "23";

    char *recipe_url = "http://localhost";
    char *task_id = "foobar";

    g_setenv(RECIPE_URL, recipe_url, TRUE);
    g_setenv(TASK_ID, task_id, TRUE);

    char *argv[] = {
        CMD_RHTS,
        test_name,
        test_result,
        logfile,
        score
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_true(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->outputfile, ==, logfile);
    g_assert_cmpstr(app_data->score, ==, score);

    g_unsetenv(RECIPE_URL);
    g_unsetenv(TASK_ID);

    restraint_free_appdata(app_data);
}

/* One arg is not valid for rhts */
static void
test_rhts_small_argcount()
{
    AppData *app_data = restraint_create_appdata();
    char *argv[] = {
        CMD_RHTS,
        "foo"
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_false(rc);

    restraint_free_appdata(app_data);
}

/* 5 args is not valid for rhts */
static void
test_rhts_large_argcount()
{
    AppData *app_data = restraint_create_appdata();
    char *argv[] = {
        CMD_RHTS,
        "foo",
        "bar",
        "baz",
        "bob",
        "alice"
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_false(rc);

    restraint_free_appdata(app_data);
}

static void
test_rstrnt_2arg()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "my_restraint_test";
    char *test_result = "ALL GOOD";
    char *outputfile = "output.log";

    char *argv[] = {
        CMD_RSTRNT,
        "--server",
        "https://localhost/",
        "-o",
        outputfile,
        test_name,
        test_result,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_false(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->outputfile, ==, outputfile);

    restraint_free_appdata(app_data);
}

static void
test_rstrnt_3arg()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "my_restraint_test";
    char *test_result = "ALL GOOD";
    char *score = "23";
    char *outputfile = "output.log";

    char *argv[] = {
        CMD_RSTRNT,
        "--server",
        "https://localhost/",
        "--outputfile",
        outputfile,
        test_name,
        test_result,
        score,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_false(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->outputfile, ==, outputfile);
    g_assert_cmpstr(app_data->score, ==, score);

    restraint_free_appdata(app_data);
}

/* 1 arg is not valid for restraint */
static void
test_rstrnt_small_argcount()
{
    AppData *app_data = restraint_create_appdata();
    char *argv[] = {
        CMD_RSTRNT,
        "-o",
        "https://localhost",
        "foo"
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_false(rc);

    restraint_free_appdata(app_data);
}

/* 4 args is not valid for restraint */
static void
test_rstrnt_large_argcount()
{
    AppData *app_data = restraint_create_appdata();
    char *argv[] = {
        CMD_RSTRNT,
        "-o",
        "https://localhost",
        "foo"
        "bar",
        "baz",
        "bob"
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_false(rc);

    restraint_free_appdata(app_data);
}

/*
    In RHTS mode, we should accept positional arguments that start with dashes,
    without needing to pass in the "--".  This maintains compatability
    with rhts-report-result.
*/
static void
test_names_with_dashes_rhts()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "-my_restraint_test";
    char *test_result = "-ALL GOOD";
    char *score = "-23";
    char *logfile = "mylogfile.log";

    char *recipe_url = "http://localhost";
    char *task_id = "foobar";

    g_setenv(RECIPE_URL, recipe_url, TRUE);
    g_setenv(TASK_ID, task_id, TRUE);

    char *argv[] = {
        CMD_RHTS,
        test_name,
        test_result,
        logfile,
        score,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_true(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->score, ==, score);
    g_assert_cmpstr(app_data->outputfile, ==, logfile);

    g_unsetenv(RECIPE_URL);
    g_unsetenv(TASK_ID);

    restraint_free_appdata(app_data);
}

static void
test_names_with_dashes_rstrnt()
{
    AppData *app_data = restraint_create_appdata();

    char *test_name = "-my_restraint_test";
    char *test_result = "-ALL GOOD";
    char *score = "-23";
    char *message = "my message";

    char *argv[] = {
        CMD_RSTRNT,
        "-s",
        "https://localhost/",
        "-t",
        message,
        "--",
        test_name,
        test_result,
        score,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);

    g_assert_false(app_data->rhts_compat);
    g_assert_cmpstr(app_data->test_name, ==, test_name);
    g_assert_cmpstr(app_data->test_result, ==, test_result);
    g_assert_cmpstr(app_data->score, ==, score);
    g_assert_cmpstr(app_data->result_msg, ==, message);

    restraint_free_appdata(app_data);
}

static void
test_environment_variables()
{
    AppData *app_data = restraint_create_appdata();

    char *outputfile = "output.log";
    char *recipe_url = "http://localhost/";
    char *task_id = "my task id";

    char *test_name = "my_restraint_test";
    char *test_result = "ALL GOOD";
    char *score = "23";

    /* This is the expected URL to be generated from the env vars */
    char *server_url = g_strdup_printf(
        "%s/tasks/%s/results/",recipe_url, task_id);

    g_setenv(OUTPUTFILE, outputfile, TRUE);
    g_setenv(RECIPE_URL, recipe_url, TRUE);
    g_setenv(TASK_ID, task_id, TRUE);

    char *argv[] = {
        CMD_RSTRNT,
        test_name,
        test_result,
        score,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);
    g_assert_cmpstr(app_data->outputfile, ==, outputfile);
    g_assert_cmpstr(app_data->task_id, ==, task_id);
    g_assert_cmpstr(app_data->server_recipe, ==, recipe_url);
    g_assert_cmpstr(app_data->server, ==, server_url);

    g_unsetenv(OUTPUTFILE);
    g_unsetenv(RECIPE_URL);
    g_unsetenv(TASK_ID);

    g_free(server_url);

    restraint_free_appdata(app_data);
}

static void
test_prefix_variables()
{
    AppData *app_data = restraint_create_appdata();

    char *outputfile = "output.log";
    char *recipe_url = "http://localhost/";
    char *task_id = "my task id";
    char *prefix = "VAR_PREFIX_";

    char *test_name = "my_restraint_test";
    char *test_result = "ALL GOOD";
    char *score = "23";

    /* This is the expected URL to be generated from the env vars */
    char *server_url = g_strdup_printf(
        "%s/tasks/%s/results/", recipe_url, task_id);

    char *prefixed_recipe_url =
        g_strdup_printf("%s%s", prefix, RECIPE_URL);
    char *prefixed_task_id =
        g_strdup_printf("%s%s", prefix, TASK_ID);

    g_setenv(OUTPUTFILE, outputfile, TRUE);
    g_setenv(prefixed_recipe_url, recipe_url, TRUE);
    g_setenv(prefixed_task_id, task_id, TRUE);
    g_setenv(HARNESS_PREFIX, prefix, TRUE);

    char *argv[] = {
        CMD_RSTRNT,
        test_name,
        test_result,
        score,
    };
    int argc = sizeof(argv) / sizeof(char*);

    gboolean rc = parse_arguments(app_data, argc, argv);
    g_assert_true(rc);
    g_assert_cmpstr(app_data->outputfile, ==, outputfile);
    g_assert_cmpstr(app_data->task_id, ==, task_id);
    g_assert_cmpstr(app_data->server_recipe, ==, recipe_url);
    g_assert_cmpstr(app_data->server, ==, server_url);

    g_unsetenv(OUTPUTFILE);
    g_unsetenv(prefixed_recipe_url);
    g_unsetenv(prefixed_task_id);
    g_unsetenv(HARNESS_PREFIX);

    g_free(prefixed_recipe_url);
    g_free(prefixed_task_id);
    g_free(server_url);

    restraint_free_appdata(app_data);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/cmd_upload/rhts_3arg", test_rhts_3arg);
    g_test_add_func("/cmd_upload/rhts_4arg", test_rhts_4arg);
    g_test_add_func("/cmd_upload/rhts_large_argcount", test_rhts_large_argcount);
    g_test_add_func("/cmd_upload/rhts_small_argcount", test_rhts_small_argcount);
    g_test_add_func("/cmd_upload/rstrnt_2arg", test_rstrnt_2arg);
    g_test_add_func("/cmd_upload/rstrnt_3arg", test_rstrnt_3arg);
    g_test_add_func("/cmd_upload/rstrnt_large_argcount", test_rstrnt_large_argcount);
    g_test_add_func("/cmd_upload/rstrnt_small_argcount", test_rstrnt_small_argcount);
    g_test_add_func("/cmd_upload/args_with_dashes_rstrnt", test_names_with_dashes_rstrnt);
    g_test_add_func("/cmd_upload/args_with_dashes_rhts", test_names_with_dashes_rhts);
    g_test_add_func("/cmd_upload/environment_vars", test_environment_variables);
    g_test_add_func("/cmd_upload/prefixed_environment_vars", test_prefix_variables);

    return g_test_run();
}
