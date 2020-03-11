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

/*
 * Ensure that get_package_version returns the version of a package
 * installed in the system.
 */
void
test_get_package_version_installed (void)
{
    GError *error;
    gchar  *version;

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

    g_test_add_func ("/utils/get_package_version/not_installed",
                     test_get_package_version_not_installed);
    g_test_add_func ("/utils/get_package_version/installed",
                     test_get_package_version_installed);

    return g_test_run ();
}
