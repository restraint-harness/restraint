#include <glib.h>

#include "cmd_utils.h"
#include "errors.h"

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

static void
test_rstrnt_getenv_not_set (void)
{
    g_assert_cmpstr (rstrnt_getenv ("NOT_SET_PLEASE"), ==, NULL);
}

static void
test_rstrnt_getenv_empty (void)
{
    const gchar *value;

    g_setenv ("EMPTY_PLEASE", "", TRUE);

    value = rstrnt_getenv ("EMPTY_PLEASE");

    g_unsetenv ("EMPTY_PLEASE");

    g_assert_cmpstr (value, ==, "");
}

static void
test_rstrnt_getenv_set (void)
{
    const gchar *value;

    g_setenv ("SOME_VAR", "42", TRUE);

    value = rstrnt_getenv ("SOME_VAR");

    g_unsetenv ("SOME_VAR");

    g_assert_cmpstr (value, ==, "42");
}

static void
test_rstrnt_getenv_prefix (void)
{
    const gchar *value;

    g_setenv ("HARNESS_PREFIX", "PREFIX_", TRUE);
    g_setenv ("PREFIX_SOME_VAR", "42", TRUE);

    value = rstrnt_getenv ("SOME_VAR");

    g_unsetenv ("HARNESS_PREFIX");
    g_unsetenv ("PREFIX_SOME_VAR");

    g_assert_cmpstr (value, ==, "42");
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/cmd_utils/rstrnt_getenv/not_set",
                     test_rstrnt_getenv_not_set);
    g_test_add_func ("/cmd_utils/rstrnt_getenv/empty",
                     test_rstrnt_getenv_empty);
    g_test_add_func ("/cmd_utils/rstrnt_getenv/set",
                     test_rstrnt_getenv_set);
    g_test_add_func ("/cmd_utils/rstrnt_getenv/prefix",
                     test_rstrnt_getenv_prefix);

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

    return g_test_run ();
}
