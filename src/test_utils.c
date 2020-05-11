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
#include "utils.h"

static gboolean
has_rpm_program (void)
{
    gchar *rpm_path;

    rpm_path = g_find_program_in_path ("rpm");

    if (rpm_path == NULL)
        return FALSE;

    g_free (rpm_path);

    return TRUE;
}

/*
 * Ensure that error is handled when RPM program in not available
 */
static void
test_get_package_version_no_rpm_cmd (void)
{
    GError *error;
    gchar  *expected_msg_regex;
    gchar  *path_bkp;
    gchar  *version;

    path_bkp = g_strdup (g_getenv ("PATH"));
    g_setenv ("PATH", "", TRUE);

    g_assert_false (has_rpm_program ());

    error = NULL;

    version = get_package_version ("some-package", &error);

    g_setenv ("PATH", path_bkp, TRUE);

    g_assert_null (version);
    g_assert_nonnull (error);

    expected_msg_regex = "Failed to spawn command:.+";
    g_assert_true (g_regex_match_simple (expected_msg_regex, error->message, 0, 0));

    g_clear_error (&error);
    g_free (path_bkp);
}

/*
 * Ensure that get_package_version returns the version of a package
 * installed in the system.
 */
void
test_get_package_version_installed (void)
{
    GError *error;
    gchar  *version;

    if (!has_rpm_program ())
        return g_test_skip ("RPM program not available");

    error = NULL;

    /* Using make as package name should guarantee success, as it's used
       to run the test suite.
       TODO: Mock g_spawn_command_line_sync */
    version = get_package_version ("make", &error);

    g_assert_null (error);
    g_assert_nonnull (version);
    g_assert_true (g_regex_match_simple ("^[0-9.]+$", version, 0, 0));

    g_free (version);
}

/*
 * Ensure that get_package_version returns NULL and sets error if the
 * package is not installed in the system.
 */
void
test_get_package_version_not_installed (void)
{
    GError *error;
    gchar  *expected_message;
    gchar  *version;

    if (!has_rpm_program ())
        return g_test_skip ("RPM program not available");

    expected_message = "Get version command: "
        "rpm -q --qf '%{Version}' _the-package-that-doesnt-exist_, "
        "returned error: package _the-package-that-doesnt-exist_ is not installed\n";

    error = NULL;

    /* TODO: Mock g_spawn_command_line_sync */
    version = get_package_version ("_the-package-that-doesnt-exist_", &error);

    g_assert_null (version);
    g_assert_nonnull (error);
    g_assert_cmpstr (error->message, ==, expected_message);
    g_clear_error (&error);
}

/*
 * Ensure that get_package_version error handling favors message in
 * stdout over stderr.
 */
void
test_get_package_version_stderr (void)
{
    GError       *error;
    const gchar  *path_bkp;
    gchar        *expected_message;
    gchar        *version;

    if (!has_rpm_program ())
        return g_test_skip ("RPM program not available");

    path_bkp = g_getenv ("PATH");
    g_setenv ("PATH", "./test-dummies/cmd_utils/bin", TRUE);

    g_setenv ("MOCK_RPM_STDOUT", "Message in STDOUT", TRUE);
    g_setenv ("MOCK_RPM_STDERR", "Message in STDERR", TRUE);
    g_setenv ("MOCK_RPM_EXIT", "1", TRUE);

    expected_message = "Get version command: "
        "rpm -q --qf '%{Version}' rpm-is-mocked, "
        "returned error: Message in STDOUT\n";

    error = NULL;

    version = get_package_version ("rpm-is-mocked", &error);

    g_setenv ("PATH", path_bkp, TRUE);
    g_unsetenv ("MOCK_RPM_STDOUT");
    g_unsetenv ("MOCK_RPM_STDERR");
    g_unsetenv ("MOCK_RPM_EXIT");

    g_assert_null (version);
    g_assert_nonnull (error);
    g_assert_cmpstr (error->message, ==, expected_message);
    g_clear_error (&error);
}

static gboolean
check_env_file_present (guint port)
{
    gchar *filename = get_envvar_filename(port);
    gboolean result = g_file_test(filename, G_FILE_TEST_EXISTS);
    g_free(filename);
    return(result);
}

/*
 * Checks the content of environment file. Returns number of
 * expected elements found.
 */
static guint
check_env_vars (guint port)
{
    int i = 0;
    gchar *msgbuf = NULL;
    gchar *filename = get_envvar_filename(port);
    GError *error = NULL;

    g_file_get_contents(filename, &msgbuf, NULL, &error);
    g_free(filename);
    g_assert_no_error(error);

    gchar **myarr = g_strsplit(msgbuf, "\n", -1);
    g_free(msgbuf);

    for (i = 0; myarr[i] != NULL; i++) {

        if (strlen(myarr[i]) != 0) {
            gchar **my_vars = g_strsplit(myarr[i], "=", 2);
            if (g_strcmp0(my_vars[0], "HARNESS_PREFIX") == 0) {
                g_assert(g_strcmp0(my_vars[1], "RSTRNT_") == 0);
            } else if (g_strcmp0(my_vars[0], "RSTRNT_RECIPE_URL") == 0) {
                g_assert(g_strcmp0(my_vars[1],
                         "http://localhost:46344/recipes/123") == 0);
            } else if (g_strcmp0(my_vars[0], "RSTRNT_TASKID") == 0) {
                g_assert(g_strcmp0(my_vars[1], "456") == 0);
            } else {
                // assert if something other than expected env vars
                g_assert(g_strcmp0(my_vars[0], "RSTRNT_URL") == 0);
                g_assert(g_strcmp0(my_vars[1], "http://localhost:46344") == 0);
            }
            g_strfreev(my_vars);
        }
    }
    g_strfreev(myarr);

    return (i);

}

/*
 * Ensure creation/content/removal of environment file
 * works properly.
 */
void
test_environment_file (void)
{
    GError *error = NULL;
    guint port = 46344;

    update_env_file("RSTRNT_", "http://localhost:46344", "123", "456",
                    port, &error);
    g_assert_no_error(error);
    g_assert_true(check_env_file_present(port));

    // Validate file content
    g_assert(check_env_vars(port) == 5);

    remove_env_file(port);
    g_assert_false(check_env_file_present(port));
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/utils/get_package_version/no_rpm_cmd",
                     test_get_package_version_no_rpm_cmd);
    g_test_add_func ("/utils/get_package_version/not_installed",
                     test_get_package_version_not_installed);
    g_test_add_func ("/utils/get_package_version/installed",
                     test_get_package_version_installed);
    g_test_add_func ("/utils/get_package_version/stderr",
                     test_get_package_version_stderr);
    g_test_add_func ("/utils/test_environment_file",
                     test_environment_file);

    return g_test_run ();
}
