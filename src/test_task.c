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

#include "task.c"

SoupSession *soup_session;

gchar *tmp_test_dir = NULL;

static void
assert_offset (GHashTable  *offsets,
               const gchar *path,
               goffset      expected_value)
{
    goffset *offset;

    offset = g_hash_table_lookup (offsets, path);

    g_assert_nonnull (offset);
    g_assert_cmpint (*offset, ==, expected_value);
}

static void
test_parse_task_config_file_exists (void)
{
    Task   *task;
    GError *err = NULL;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    g_assert_true (parse_task_config ("test-data/task42.conf", task, &err));

    g_assert_no_error (err);

    g_assert_cmpint (task->reboots, ==, 1);
    g_assert_nonnull (task->offsets);
    g_assert_cmpint (g_hash_table_size (task->offsets), ==, 2);
    assert_offset (task->offsets, "logs/taskout.log", 42);
    assert_offset (task->offsets, "logs/harness.log", 58);
    g_assert_true (task->started);
    g_assert_true (task->localwatchdog);

    restraint_task_free (task);
}


static void
test_parse_task_config_no_file (void)
{
    Task   *task;
    GError *err = NULL;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    g_assert_true (parse_task_config ("there/is/no/file", task, &err));

    g_assert_no_error (err);

    g_assert_cmpint (task->reboots, ==, 0);
    g_assert_nonnull (task->offsets);
    g_assert_cmpint (g_hash_table_size (task->offsets), ==, 0);
    g_assert_false (task->started);
    g_assert_false (task->localwatchdog);

    restraint_task_free (task);
}

static void
test_parse_task_config_bad_file (void)
{
    Task               *task;
    g_autoptr (GError)  err = NULL;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    g_assert_false (parse_task_config ("test-data/bad.conf", task, &err));

    g_assert_error (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

    restraint_task_free (task);
}

static void
test_restraint_task_new (void)
{
    Task *task;

    task = restraint_task_new ();

    g_assert_nonnull (task);
    g_assert_cmpint (task->remaining_time, ==, -1);
    g_assert_nonnull (task->offsets);
    g_assert_cmpint (g_hash_table_size (task->offsets), ==, 0);

    g_hash_table_destroy (task->offsets);
    g_slice_free (Task, task);
}

static void
test_restraint_task_free (void)
{
    Task *task = NULL;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    restraint_task_free (task);
}

static void
test_restraint_task_get_offset (void)
{
    Task        *task;
    const gchar *path;
    goffset     *offset;

    path = "/some/random/path.log";

    task = restraint_task_new ();

    g_assert_nonnull (task);

    offset = restraint_task_get_offset (task, path);

    assert_offset (task->offsets, path, 0);

    g_assert_true (offset == restraint_task_get_offset (task, path));

    restraint_task_free (task);
}

static void
assert_key_file_offset (Task        *task,
                        const gchar *path,
                        const gchar *key,
                        goffset      expected_value)
{
    g_autofree gchar     *section = NULL;
    g_autoptr (GError)    err = NULL;
    g_autoptr (GKeyFile)  key_file = NULL;
    goffset               value;

    key_file = g_key_file_new ();

    g_assert_true (g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &err));
    g_assert_no_error (err);

    section = g_strdup_printf ("offsets_%s", task->task_id);

    value = g_key_file_get_int64 (key_file, section, key, &err);

    g_assert_no_error (err);
    g_assert_cmpint (value, ==, expected_value);
}

static void
test_task_config_set_offset (void)
{
    Task               *task;
    const gchar        *path;
    g_autofree gchar   *config_file;
    g_autoptr (GError)  err = NULL;
    goffset             value;

    config_file = g_build_filename (tmp_test_dir, "config.conf", NULL);
    path = "/some/random/path.log";
    value = 42;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup_printf ("%" G_GINT64_FORMAT, g_get_real_time ());

    g_assert_true (task_config_set_offset (config_file, task, path, value, &err));
    g_assert_no_error (err);
    assert_key_file_offset (task, config_file, path, value);

    restraint_task_free (task);
    g_remove (config_file);
}

static void
test_task_config_get_offsets_file_exists (void)
{
    GError *err = NULL;
    Task   *task;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    task_config_get_offsets ("test-data/task42.conf", task, &err);

    g_assert_no_error (err);

    g_assert_cmpint (g_hash_table_size (task->offsets), ==, 2);
    assert_offset (task->offsets, "logs/taskout.log", 42);
    assert_offset (task->offsets, "logs/harness.log", 58);

    restraint_task_free (task);
}

static void
test_task_config_get_offsets_no_file (void)
{
    GError *err = NULL;
    Task   *task;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    task_config_get_offsets ("there/is/no/file", task, &err);

    g_assert_no_error (err);

    g_assert_cmpint (g_hash_table_size (task->offsets), ==, 0);

    restraint_task_free (task);
}

static void
test_task_config_get_offsets_bad_file (void)
{
    Task               *task;
    g_autoptr (GError)  err = NULL;

    task = restraint_task_new ();

    g_assert_nonnull (task);

    task->task_id = g_strdup ("42");

    task_config_get_offsets ("test-data/bad.conf", task, &err);

    g_assert_error (err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);

    restraint_task_free (task);
}


int
main (int   argc,
      char *argv[])
{
    gboolean success;

    tmp_test_dir = g_dir_make_tmp ("test_task_XXXXXX", NULL);

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/task/restraint_task_new", test_restraint_task_new);
    g_test_add_func ("/task/restraint_task_free", test_restraint_task_free);
    g_test_add_func ("/task/parse_task_config/no_file", test_parse_task_config_no_file);
    g_test_add_func ("/task/parse_task_config/file_exists", test_parse_task_config_file_exists);
    g_test_add_func ("/task/parse_task_config/bad_file", test_parse_task_config_bad_file);
    g_test_add_func ("/task/restraint_task_get_offset", test_restraint_task_get_offset);
    g_test_add_func ("/task/task_config_set_offset", test_task_config_set_offset);
    g_test_add_func ("/task/task_config_get_offsets/file_exists", test_task_config_get_offsets_file_exists);
    g_test_add_func ("/task/task_config_get_offsets/no_file", test_task_config_get_offsets_no_file);
    g_test_add_func ("/task/task_config_get_offsets/bad_file", test_task_config_get_offsets_bad_file);

    success = g_test_run ();

    g_remove (tmp_test_dir);
    g_free (tmp_test_dir);

    return success;
}
