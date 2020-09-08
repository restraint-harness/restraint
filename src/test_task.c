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

#include "task.c"

SoupSession *soup_session;

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

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/task/restraint_task_new", test_restraint_task_new);
    g_test_add_func ("/task/restraint_task_free", test_restraint_task_free);
    g_test_add_func ("/task/parse_task_config/no_file", test_parse_task_config_no_file);
    g_test_add_func ("/task/parse_task_config/file_exists", test_parse_task_config_file_exists);
    g_test_add_func ("/task/parse_task_config/bad_file", test_parse_task_config_bad_file);

    return g_test_run ();
}
