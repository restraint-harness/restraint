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
    GError *error;
    gint pid_result;
    gboolean localwatchdog;
    GMainLoop *loop;
} RunData;

void
process_finish_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    RunData *run_data = (RunData *) user_data;
    run_data->error = error;
    run_data->pid_result = pid_result;
    run_data->localwatchdog = localwatchdog;
    g_main_loop_quit (run_data->loop);
    g_main_loop_unref (run_data->loop);
}

static void test_process_success(void) {
    RunData *run_data;
    const gchar *command = "true";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    process_run (command,
                 NULL,
                 NULL,
                 0,
                 NULL,
                 process_finish_callback,
                 NULL,
                 run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_slice_free (RunData, run_data);
}

static void test_process_failure(void) {
    RunData *run_data;
    const gchar *command = "false";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    process_run (command,
                 NULL,
                 NULL,
                 0,
                 NULL,
                 process_finish_callback,
                 NULL,
                 run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpint (run_data->pid_result, !=, 0);
    g_slice_free (RunData, run_data);
}

static void test_watchdog_success(void) {
    RunData *run_data;
    // Watchdog success so command time < max time
    const guint64 maximumtime = 2;
    const gchar *command = "sleep 1";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    process_run (command,
                 NULL,
                 NULL,
                 maximumtime,
                 NULL,
                 process_finish_callback,
                 NULL,
                 run_data);
    
    // run event loop while process is running.
    g_main_loop_run (run_data->loop);
    
    // process finished, check our results.
    // g_assert fails if false arg so need ! 
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert (!run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_slice_free (RunData, run_data);
}

static void test_watchdog_failure(void) {
    RunData *run_data;
    // Watchdog fail to command time > max time
    const guint64 maximumtime = 1;
    const gchar *command = "sleep 2";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    process_run (command,
                 NULL,
                 NULL,
                 maximumtime,
                 NULL,
                 process_finish_callback,
                 NULL,
                 run_data);
    
    // run event loop while process is running.
    g_main_loop_run (run_data->loop);
    
    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert (run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, !=, 0);
    g_slice_free (RunData, run_data);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/process/success", test_process_success);
    g_test_add_func("/process/failure", test_process_failure);
    g_test_add_func("/process/watchdog_pass", test_watchdog_success);
    g_test_add_func("/process/watchdog_failure", test_watchdog_failure);
    return g_test_run();
}
