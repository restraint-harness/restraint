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

#include "config.h"

gchar *tmp_test_dir = NULL;

static void
assert_key_val (const gchar *file,
                const gchar *section,
                const gchar *key,
                const gchar *expected_val)
{
    g_autofree gchar *val = NULL;
    g_autoptr (GError) err = NULL;
    g_autoptr (GKeyFile) key_file = NULL;

    key_file = g_key_file_new ();
    g_assert_true (g_key_file_load_from_file (key_file, file, G_KEY_FILE_NONE, &err));
    g_assert_no_error (err);

    val = g_key_file_get_string (key_file, section, key, &err);

    g_assert_no_error (err);
    g_assert_cmpstr (val, ==, expected_val);
}

static void
test_restraint_config_set_types (void)
{
    const gchar *section;
    g_autofree gchar *config_file = NULL;
    g_autoptr (GError) err = NULL;
    guint64 value;

    config_file = g_strdup_printf ("%s/types.conf", tmp_test_dir);
    section = "types";
    value = 42;

    restraint_config_set (config_file, section, "uint64", &err, G_TYPE_UINT64, value);

    g_assert_no_error (err);
    assert_key_val (config_file, section, "uint64", "42");

    restraint_config_set (config_file, section, "int", &err, G_TYPE_INT, (int) value);

    g_assert_no_error (err);
    assert_key_val (config_file, section, "int", "42");

    restraint_config_set (config_file, section, "string", &err, G_TYPE_STRING, "the-string");

    g_assert_no_error (err);
    assert_key_val (config_file, section, "string", "the-string");

    restraint_config_set (config_file, section, "bool", &err, G_TYPE_BOOLEAN, TRUE);

    g_assert_no_error (err);
    assert_key_val (config_file, section, "bool", "true");

    g_remove (config_file);
}

static void
test_restraint_config_set_remove (void)
{
    const gchar *section;
    g_autofree gchar *config_file = NULL;
    g_autoptr (GError) err = NULL;
    g_autoptr (GKeyFile) key_file = NULL;

    config_file = g_strdup_printf ("%s/remove.conf", tmp_test_dir);
    section = "remove";

    key_file = g_key_file_new ();
    g_key_file_set_string (key_file, section, "key1", "val1");
    g_key_file_set_string (key_file, section, "key2", "val2");
    g_key_file_save_to_file (key_file, config_file, &err);
    g_assert_no_error (err);

    restraint_config_set (config_file, section, "key1", &err, -1);

    g_assert_no_error (err);
    g_assert_true (g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_NONE, &err));
    g_assert_no_error (err);
    g_assert_false (g_key_file_has_key (key_file, section, "key1", &err));
    g_assert_no_error (err);
    g_assert_true (g_key_file_has_key (key_file, section, "key2", &err));

    restraint_config_set (config_file, section, NULL, &err, -1);

    g_assert_no_error (err);
    g_assert_true (g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_NONE, &err));
    g_assert_no_error (err);
    g_assert_false (g_key_file_has_group (key_file, section));

    g_remove (config_file);
}

int
main (int   argc,
      char *argv[])
{
    gboolean success;

    tmp_test_dir = g_dir_make_tmp ("test_config_XXXXXX", NULL);

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/config/set/types", test_restraint_config_set_types);
    g_test_add_func ("/config/set/remove", test_restraint_config_set_remove);

    success = g_test_run ();

    g_remove (tmp_test_dir);
    g_free (tmp_test_dir);

    return success;
}
