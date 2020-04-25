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
#include <string.h>

#include "process.h"
#include "errors.h"

typedef struct {
    GError *error;
    gint pid_result;
    gboolean localwatchdog;
    GMainLoop *loop;
    GString *output;
} RunData;

gboolean
test_process_io_cb (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    GError *error = NULL;

    gchar buf[131072];
    gsize bytes_read;

    if (condition & (G_IO_IN )) {
        switch (g_io_channel_read_chars(io, buf, 131072, &bytes_read, &error)) {
          case G_IO_STATUS_NORMAL:
            g_string_append_len (run_data->output, buf, bytes_read);
            return TRUE;

          case G_IO_STATUS_ERROR:
             g_clear_error (&error);
             return FALSE;

          case G_IO_STATUS_EOF:
             g_string_append_len (run_data->output, "finished!\n", 10);
             return FALSE;

          case G_IO_STATUS_AGAIN:
             g_string_append_len (run_data->output, "Not ready.. try again.\n", 24);
             return TRUE;

          default:
             g_return_val_if_reached(FALSE);
             break;
        }
    }
    return FALSE;
}

void
test_process_finish_cb (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
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
                 FALSE,
                 0,
                 NULL,
                 NULL,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
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
                 FALSE,
                 0,
                 NULL,
                 NULL,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
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
                 FALSE,
                 maximumtime,
                 NULL,
                 NULL,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
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
                 FALSE,
                 maximumtime,
                 NULL,
                 NULL,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
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

gboolean hang_quit_loop(gpointer data) {
    RunData *run_data = (RunData*) data;
    g_set_error(&run_data->error, RESTRAINT_ERROR,
                RESTRAINT_TASK_RUNNER_EXEC_ERROR,
                "Execution timed out");
    g_main_loop_quit(run_data->loop);
    return FALSE;
}

static void test_no_hang(void) {
    RunData *run_data;
    // Watchdog fail to command time > max time
    const guint64 maximumtime = 60;
    const gchar *command = "hang_test";
    gchar *expected = "foo\nbar\n";
    GString *output = g_string_new(NULL);
    gchar **outsplit = NULL;

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 FALSE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 run_data);

    guint toid = g_timeout_add(20000, hang_quit_loop, run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_source_remove(toid);
    outsplit = g_strsplit(run_data->output->str, "\n", -1);
    for (guint32 i = 0; i < g_strv_length(outsplit); i++) {
        gchar *line = outsplit[i];
        if (g_strcmp0(line, "") != 0 && strncmp(line,
                "Initializing built-in extension", 31) != 0) {
            g_string_append_printf(output, "%s\n", line);
        }
    }
    g_strfreev(outsplit);

    g_assert_cmpstr(output->str, == , expected);
    g_string_free (output, TRUE);
    g_string_free (run_data->output, TRUE);
    g_assert (!run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_slice_free (RunData, run_data);
}

static void test_use_pty(void) {
    RunData *run_data;
    // Watchdog fail to command time > max time
    const guint64 maximumtime = 60;
    const gchar *command = "is_tty";
    gchar *expected = "True\r\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 TRUE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_assert (!run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_slice_free (RunData, run_data);
}

static void test_dont_use_pty(void) {
    RunData *run_data;
    const guint64 maximumtime = 60;
    const gchar *command = "is_tty";
    gchar *expected = "False\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 FALSE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_assert (!run_data->localwatchdog);
    g_assert_cmpint (run_data->pid_result, !=, 0);
    g_slice_free (RunData, run_data);
}

static void
test_process_read_empty_stdin (void)
{
    RunData *run_data;
    gchar   *command;
    gchar   *expected;
    guint64  maximumtime;

    maximumtime = 3;
    command = "cat";
    expected = "";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 FALSE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 run_data);

    g_main_loop_run (run_data->loop);

    g_assert_no_error (run_data->error);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_assert_cmpstr (run_data->output->str, == , expected);
    g_assert (!run_data->localwatchdog);

    g_string_free (run_data->output, TRUE);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
}

static void
test_process_read_content_input (void)
{
    RunData *run_data;
    gchar   *command;
    gchar   *expected;
    guint64  maximumtime;

    maximumtime = 3;
    command = "cat";
    expected = "Some text for stdin\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 FALSE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 expected,
                 strlen (expected),
                 FALSE,
                 NULL,
                 run_data);

    g_main_loop_run (run_data->loop);

    g_assert_no_error (run_data->error);
    g_assert_cmpint (run_data->pid_result, ==, 0);
    g_assert_cmpstr (run_data->output->str, == , expected);
    g_assert (!run_data->localwatchdog);

    g_string_free (run_data->output, TRUE);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
}

static void
test_process_read_empty_stdin_pty (void)
{
    RunData *run_data;
    gchar   *command;
    gchar   *expected;
    guint64  maximumtime;

    maximumtime = 3;
    command = "cat";
    expected = "";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    process_run (command,
                 NULL,
                 NULL,
                 TRUE,
                 maximumtime,
                 NULL,
                 test_process_io_cb,
                 test_process_finish_cb,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 run_data);

    g_main_loop_run (run_data->loop);

    g_assert_no_error (run_data->error);
    g_assert_cmpint (run_data->pid_result, ==, 9);
    g_assert_cmpstr (run_data->output->str, == , expected);
    g_assert (run_data->localwatchdog);

    g_string_free (run_data->output, TRUE);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/process/success", test_process_success);
    g_test_add_func("/process/failure", test_process_failure);
    g_test_add_func("/process/watchdog_pass", test_watchdog_success);
    g_test_add_func("/process/watchdog_failure", test_watchdog_failure);
    g_test_add_func("/process/no_hang", test_no_hang);
    g_test_add_func("/process/use_pty", test_use_pty);
    g_test_add_func("/process/dont_use_pty", test_dont_use_pty);

    g_test_add_func ("/process/read_content_input", test_process_read_content_input);
    g_test_add_func ("/process/read_empty_stdin", test_process_read_empty_stdin);
    g_test_add_func ("/process/read_empty_stdin_pty", test_process_read_empty_stdin_pty);

    return g_test_run();
}
