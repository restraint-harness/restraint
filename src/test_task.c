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

    return g_test_run ();
}
