#include <glib.h>

#include "cmd_utils.h"
#include "errors.h"

static void
test_get_restraintd_pid_success (void)
{
    gchar  *path_bkp;
    gint    restraintd_pid;
    GError *error;

    path_bkp = g_strdup (g_getenv ("PATH"));
    g_setenv ("PATH", "./test-dummies/cmd_utils/bin", TRUE);

    g_setenv ("MOCK_PGREP_STDOUT", "123", TRUE);
    g_setenv ("MOCK_PGREP_EXIT", "0", TRUE);

    error = NULL;

    restraintd_pid = get_restraintd_pid (&error);

    g_setenv ("PATH", path_bkp, TRUE);

    g_unsetenv ("MOCK_PGREP_STDOUT");
    g_unsetenv ("MOCK_PGREP_EXIT");

    g_assert_no_error (error);
    g_assert_cmpint (restraintd_pid, ==, 123);

    g_free (path_bkp);
}

static void
test_get_restraintd_pid_many_pids (void)
{
    GError *error;
    gchar  *expected_regex;
    gchar  *path_bkp;
    gint    restraintd_pid;

    path_bkp = g_strdup (g_getenv ("PATH"));
    g_setenv ("PATH", "./test-dummies/cmd_utils/bin", TRUE);

    g_setenv ("MOCK_PGREP_STDOUT", "123 124 125", TRUE);
    g_setenv ("MOCK_PGREP_EXIT", "0", TRUE);

    expected_regex = "Due to multiple restraintd .+";

    error = NULL;

    restraintd_pid = get_restraintd_pid (&error);

    g_setenv ("PATH", path_bkp, TRUE);

    g_unsetenv ("MOCK_PGREP_STDOUT");
    g_unsetenv ("MOCK_PGREP_EXIT");

    g_assert_cmpint (restraintd_pid, ==, 0);
    g_assert_error (error, RESTRAINT_ERROR, RESTRAINT_TOO_MANY_RESTRAINTD_RUNNING);
    g_assert_true (g_regex_match_simple (expected_regex, error->message, 0, 0));

    g_clear_error (&error);
    g_free (path_bkp);
}

static void
test_get_restraintd_pid_spawn_fail (void)
{
    GError *error;
    gchar  *expected_msg_regex;
    gchar  *path_bkp;
    gint    restraintd_pid;

    expected_msg_regex = "Failed to spawn command: .+";

    path_bkp = g_strdup (g_getenv ("PATH"));
    g_setenv ("PATH", "", TRUE);

    error = NULL;

    restraintd_pid = get_restraintd_pid (&error);

    g_setenv ("PATH", path_bkp, TRUE);

    g_assert_cmpint (restraintd_pid, ==, 0);
    g_assert_error (error, RESTRAINT_ERROR, RESTRAINT_CMDLINE_ERROR);
    g_assert_true (g_regex_match_simple (expected_msg_regex, error->message, 0, 0));

    g_clear_error (&error);
    g_free (path_bkp);
}

static void
test_get_restraintd_pid_no_restraintd (void)
{
    GError *error;
    gint    restraintd_pid;

    error = NULL;

    /* Hopefully, there is no restraintd on the system running the tests.
     * TODO: This should be mocked too. */
    restraintd_pid = get_restraintd_pid (&error);

    g_assert_cmpint (restraintd_pid, ==, 0);
    g_assert_error (error, RESTRAINT_ERROR, RESTRAINT_NO_RESTRAINTD_RUNNING_ERROR);
    g_assert_cmpstr (error->message, ==, "Failed to get restraintd pid. Server not running.");

    g_clear_error (&error);
}

static void
test_get_taskid_set (void)
{
    gchar *task_id;
    gchar *expected_task_id;

    expected_task_id = "42";

    g_setenv ("TASKID", "42", TRUE);

    task_id = get_taskid ();

    g_unsetenv ("TASKID");

    g_assert_cmpstr (task_id, ==, expected_task_id);
}

static void
test_get_taskid_not_set (void)
{
    g_assert_null (get_taskid());
}

static void
test_get_taskid_empty (void)
{
    gchar *task_id;

    g_setenv ("TASKID", "", TRUE);

    task_id = get_taskid ();

    g_unsetenv ("TASKID");

    g_assert_cmpstr (task_id, ==, "");
}

static void
test_get_taskid_prefix (void)
{
    gchar *task_id;
    gchar *expected_task_id;

    expected_task_id = "42";

    g_setenv ("HARNESS_PREFIX", "RESTRAINT_", TRUE);
    g_setenv ("RESTRAINT_TASKID", "42", TRUE);

    task_id = get_taskid ();

    g_unsetenv ("HARNESS_PREFIX");
    g_unsetenv ("RESTRAINT_TASKID");

    g_assert_cmpstr (task_id, ==, expected_task_id);
}

static void
test_get_recipe_url_set (void)
{
    gchar *recipe_url;
    gchar *expected_recipe_url;

    expected_recipe_url = "http://localhost:8000/recipes/42";

    g_setenv ("RECIPE_URL", "http://localhost:8000/recipes/42", TRUE);

    recipe_url = get_recipe_url ();

    g_unsetenv ("RECIPE_URL");

    g_assert_cmpstr (recipe_url, ==, expected_recipe_url);
}

static void
test_get_recipe_url_not_set (void)
{
    g_assert_null (get_recipe_url());
}

static void
test_get_recipe_url_empty (void)
{
    gchar *recipe_url;

    g_setenv ("RECIPE_URL", "", TRUE);

    recipe_url = get_recipe_url ();

    g_unsetenv ("RECIPE_URL");

    g_assert_cmpstr (recipe_url, ==, "");
}

static void
test_get_recipe_url_prefix (void)
{
    gchar *recipe_url;
    gchar *expected_recipe_url;

    expected_recipe_url = "http://localhost:8000/recipes/42";

    g_setenv ("HARNESS_PREFIX", "RESTRAINT_", TRUE);
    g_setenv ("RESTRAINT_RECIPE_URL", "http://localhost:8000/recipes/42", TRUE);

    recipe_url = get_recipe_url ();

    g_unsetenv ("HARNESS_PREFIX");
    g_unsetenv ("RESTRAINT_RECIPE_URL");

    g_assert_cmpstr (recipe_url, ==, expected_recipe_url);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/cmd_utils/get_taskid/not_set",
                     test_get_taskid_not_set);
    g_test_add_func ("/cmd_utils/get_taskid/empty",
                     test_get_taskid_empty);
    g_test_add_func ("/cmd_utils/get_taskid/set",
                     test_get_taskid_set);
    g_test_add_func ("/cmd_utils/get_taskid/prefix",
                     test_get_taskid_prefix);

    g_test_add_func ("/cmd_utils/get_recipe_url/not_set",
                     test_get_recipe_url_not_set);
    g_test_add_func ("/cmd_utils/get_recipe_url/empty",
                     test_get_recipe_url_empty);
    g_test_add_func ("/cmd_utils/get_recipe_url/set",
                     test_get_recipe_url_set);
    g_test_add_func ("/cmd_utils/get_recipe_url/prefix",
                     test_get_recipe_url_prefix);

    g_test_add_func ("/cmd_utils/get_restraintd_pid/success",
                     test_get_restraintd_pid_success);
    g_test_add_func ("/cmd_utils/get_restraintd_pid/many_pids",
                     test_get_restraintd_pid_many_pids);
    g_test_add_func ("/cmd_utils/get_restraintd_pid/spawn_fail",
                     test_get_restraintd_pid_spawn_fail);
    g_test_add_func ("/cmd_utils/get_restraintd_pid/no_restraintd",
                     test_get_restraintd_pid_no_restraintd);

    return g_test_run ();
}
