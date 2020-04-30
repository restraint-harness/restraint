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

    return g_test_run ();
}
