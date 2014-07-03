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

#include "process.h"

typedef struct {
    gint pid_result;
    gboolean localwatchdog;
    GMainLoop *loop;
} RunData;

void
process_finish_callback (gint pid_result, gboolean localwatchdog, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    run_data->pid_result = pid_result;
    run_data->localwatchdog = localwatchdog;
    g_main_loop_quit (run_data->loop);
}

static void test_process_success(void) {
    CommandData *command_data;
    RunData *run_data;
    gboolean succeeded;
    GError *error = NULL;
    const gchar *command[] = { "true", NULL };

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    command_data = g_slice_new0 (CommandData);
    command_data->command = command;
    succeeded = process_run (command_data,
                             NULL,
                             process_finish_callback,
                             run_data,
                             &error);
    // check that initial request worked.
    g_assert (succeeded);
    g_assert_no_error (error);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_cmpint (run_data->pid_result, ==, 0);
}

static void test_process_failure(void) {
    CommandData *command_data;
    RunData *run_data;
    gboolean succeeded;
    GError *error = NULL;
    const gchar *command[] = { "false", NULL };

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    command_data = g_slice_new0 (CommandData);
    command_data->command = command;
    succeeded = process_run (command_data,
                             NULL,
                             process_finish_callback,
                             run_data,
                             &error);
    // check that initial request worked.
    g_assert (succeeded);
    g_assert_no_error (error);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_cmpint (run_data->pid_result, !=, 0);
}

static void test_watchdog_success(void) {
    CommandData *command_data;
    RunData *run_data;
    gboolean succeeded;
    GError *error = NULL;
    // Watchdog success so command time < max time
    const guint64 maximumtime = 30;
    const gchar *command[] = { "sleep", "25", NULL };

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    command_data = g_slice_new0 (CommandData);
    command_data->command = command;
    command_data->max_time = maximumtime;
    // What I set localwatchdog to doesn't matter, process_run alters it
    run_data->localwatchdog = FALSE;
    succeeded = process_run (command_data,
                             NULL,
                             process_finish_callback,
                             run_data,
                             &error);
    // check that initial request worked.
    g_assert (succeeded);
    g_assert_no_error (error);
    
    // run event loop while process is running.
    g_main_loop_run (run_data->loop);
    
    // process finished, check our results.
    // g_assert fails if false arg so need ! 
    g_assert (!run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, ==, 0);
}

static void test_watchdog_failure(void) {
    CommandData *command_data;
    RunData *run_data;
    gboolean succeeded;
    GError *error = NULL;
    // Watchdog fail to command time > max time
    const guint64 maximumtime = 25;
    const gchar *command[] = { "sleep", "30", NULL };

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    command_data = g_slice_new0 (CommandData);
    command_data->command = command;
    command_data->max_time = maximumtime;
    // What I set localwatchdog to doesn't matter, process_run alters it
    run_data->localwatchdog = TRUE;
    succeeded = process_run (command_data,
                             NULL,
                             process_finish_callback,
                             run_data,
                             &error);
    // check that initial request worked.
    g_assert (succeeded);
    g_assert_no_error (error);
    
    // run event loop while process is running.
    g_main_loop_run (run_data->loop);
    
    // process finished, check our results.
    g_assert (run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, !=, 0);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/process/success", test_process_success);
    g_test_add_func("/process/failure", test_process_failure);
    g_test_add_func("/process/watchdog_pass", test_watchdog_success);
    g_test_add_func("/process/watchdog_failure", test_watchdog_failure);
    return g_test_run();
}
