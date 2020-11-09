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

#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <sys/stat.h>
#include <gio/gio.h>
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

static void
test_restraint_config_get_int64 (void)
{
    gint64 value;
    gchar *config_file;
    g_autoptr (GError) err = NULL;

    /* Missing file. */
    config_file = "the/file/that/doesnt/exist";

    value = restraint_config_get_int64 (config_file, "types", "int", &err);

    /* Notice that after return, there is now way to tell that the file
       didn't exist. The code using this function expects this
       behavior. */
    g_assert_no_error (err);
    g_assert_cmpint (value, ==, 0);

    config_file = "./test-data/config_types.conf";

    /* Missing group */
    value = restraint_config_get_int64 (config_file, "neexistuje", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_cmpint (value, ==, 0);

    /* Missing key */
    value = restraint_config_get_int64 (config_file, "types", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_cmpint (value, ==, 0);

    /* Found key with proper value */
    value = restraint_config_get_int64 (config_file, "types", "int", &err);

    g_assert_no_error (err);
    g_assert_cmpint (value, ==, -42);

    /* Found key with wrong value */
    value = restraint_config_get_int64 (config_file, "types", "string", &err);

    g_assert_error (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
    g_assert_cmpint (value, ==, 0);
}

static void
test_restraint_config_get_uint64 (void)
{
    guint64 value;
    gchar *config_file;
    g_autoptr (GError) err = NULL;

    /* Missing file. */
    config_file = "the/file/that/doesnt/exist";

    value = restraint_config_get_uint64 (config_file, "types", "uint", &err);

    /* Notice that after return, there is now way to tell that the file
       didn't exist. The code using this function expects this
       behavior. */
    g_assert_no_error (err);
    g_assert_cmpuint (value, ==, 0);

    config_file = "./test-data/config_types.conf";

    /* Missing group */
    value = restraint_config_get_uint64 (config_file, "neexistuje", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_cmpuint (value, ==, 0);

    /* Missing key */
    value = restraint_config_get_uint64 (config_file, "types", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_cmpuint (value, ==, 0);

    /* Found key with proper value */
    value = restraint_config_get_uint64 (config_file, "types", "uint", &err);

    g_assert_no_error (err);
    g_assert_cmpuint (value, ==, 42);

    /* Found key with wrong value */
    value = restraint_config_get_uint64 (config_file, "types", "string", &err);

    g_assert_error (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
    g_assert_cmpuint (value, ==, 0);
}

static void
test_restraint_config_get_boolean (void)
{
    gboolean value;
    gchar *config_file;
    g_autoptr (GError) err = NULL;

    /* Missing file. */
    config_file = "the/file/that/doesnt/exist";

    value = restraint_config_get_boolean (config_file, "types", "bool", &err);

    /* Notice that after return, there is now way to tell that the file
       didn't exist. The code using this function expects this
       behavior. */
    g_assert_no_error (err);
    g_assert_false (value);

    config_file = "./test-data/config_types.conf";

    /* Missing group */
    value = restraint_config_get_boolean (config_file, "neexistuje", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_false (value);

    /* Missing key */
    value = restraint_config_get_boolean (config_file, "types", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_false (value);

    /* Found key with proper value */
    value = restraint_config_get_boolean (config_file, "types", "bool", &err);

    g_assert_no_error (err);
    g_assert_true (value);

    /* Found key with wrong value */
    value = restraint_config_get_boolean (config_file, "types", "string", &err);

    g_assert_error (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
    g_assert_false (value);
}

static void
test_restraint_config_get_string (void)
{
    g_autofree gchar *value = NULL;
    gchar *config_file;
    g_autoptr (GError) err = NULL;

    /* Missing file. */
    config_file = "the/file/that/doesnt/exist";

    value = restraint_config_get_string (config_file, "types", "string", &err);

    /* Notice that after return, there is now way to tell that the file
       didn't exist. The code using this function expects this
       behavior. */
    g_assert_no_error (err);
    g_assert_null (value);

    config_file = "./test-data/config_types.conf";

    /* Missing group */
    value = restraint_config_get_string (config_file, "neexistuje", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_null (value);

    /* Missing key */
    value = restraint_config_get_string (config_file, "types", "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_null (value);

    /* Found key with proper value */
    value = restraint_config_get_string (config_file, "types", "string", &err);

    g_assert_no_error (err);
    g_assert_nonnull (value);
    g_assert_cmpstr (value, ==, "The string value");
}

static void
test_restraint_config_get_keys (void)
{
    gchar **value;
    gchar *config_file = NULL;
    g_autoptr (GError) err = NULL;

    /* Missing file. */
    config_file = "the/file/that/doesnt/exist";

    value = restraint_config_get_keys (config_file, "types", &err);

    /* Notice that after return, there is now way to tell that the file
       didn't exist. The code using this function expects this
       behavior. */
    g_assert_no_error (err);
    g_assert_null (value);

    /* Missing group */
    config_file = "./test-data/config_types.conf";

    value = restraint_config_get_keys (config_file, "neexistuje", &err);

    g_assert_no_error (err);
    g_assert_null (value);

    /* Found group and keys */
    value = restraint_config_get_keys (config_file, "types", &err);

    g_assert_no_error (err);
    g_assert_nonnull (value);
    g_assert_nonnull (value[0]);
    /* This assumes that the keys in the array are in the same order as
       in the file, but, the function doesn't guarantee anything. */
    g_assert_cmpstr (value[0], ==, "int");
    g_assert_cmpstr (value[3], ==, "string");
    g_assert_null (value[4]);

    g_strfreev (value);
}

static void
test_restraint_config_set_mkdir (void)
{
    guint32 dir_mode;
    g_autofree gchar *config_file = NULL;
    g_autofree gchar *parent = NULL;
    g_autoptr (GError) err = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autoptr (GFileInfo) dir_info = NULL;

    parent = g_build_filename (tmp_test_dir, "subdir", NULL);
    config_file = g_build_filename (parent, "config.conf", NULL);

    restraint_config_set (config_file, "section", "key", &err, G_TYPE_STRING, "value");

    g_assert_no_error (err);
    assert_key_val (config_file, "section", "key", "value");

    dir = g_file_new_for_path (parent);
    dir_info = g_file_query_info (dir, G_FILE_ATTRIBUTE_UNIX_MODE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    g_assert_nonnull (dir_info);
    dir_mode = g_file_info_get_attribute_uint32 (dir_info, G_FILE_ATTRIBUTE_UNIX_MODE);
    g_assert_cmpuint (dir_mode, ==, (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));

    g_remove (config_file);
    g_remove (parent);
}

int
main (int   argc,
      char *argv[])
{
    gboolean success;

    tmp_test_dir = g_dir_make_tmp ("test_config_XXXXXX", NULL);

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/config/get/int64", test_restraint_config_get_int64);
    g_test_add_func ("/config/get/uint64", test_restraint_config_get_uint64);
    g_test_add_func ("/config/get/boolean", test_restraint_config_get_boolean);
    g_test_add_func ("/config/get/string", test_restraint_config_get_string);
    g_test_add_func ("/config/get/keys", test_restraint_config_get_keys);
    g_test_add_func ("/config/set/types", test_restraint_config_set_types);
    g_test_add_func ("/config/set/remove", test_restraint_config_set_remove);
    g_test_add_func ("/config/set/mkdir", test_restraint_config_set_mkdir);

    success = g_test_run ();

    g_remove (tmp_test_dir);
    g_free (tmp_test_dir);

    return success;
}
