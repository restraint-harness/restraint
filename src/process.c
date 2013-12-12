#include <glib.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pty.h>
#include "process.h"

GQuark restraint_process_error (void)
{
    return g_quark_from_static_string("restraint-process-error-quark");
}

void
process_free (ProcessData *process_data)
{
    g_return_if_fail (process_data != NULL);
    g_slice_free (ProcessData, process_data);
}

gboolean
process_run (CommandData *command_data,
             GIOFunc io_callback,
             ProcessFinishCallback finish_callback,
             gpointer user_data,
             GError **error)
{
    ProcessData *data;
    struct termios term;
    struct winsize win = {
        .ws_col = 80, .ws_row = 24,
        .ws_xpixel = 480, .ws_ypixel = 192,
    };

    data = g_slice_new (ProcessData);
    data->localwatchdog = FALSE;
    data->command_data = command_data;
    data->finish_callback = finish_callback;
    data->user_data = user_data;

    data->pid = forkpty (&data->fd, NULL, &term, &win);
    if (data->pid < 0) {
        /* Failed to fork */
        g_set_error (error, RESTRAINT_PROCESS_ERROR,
                     RESTRAINT_PROCESS_FORK_ERROR,
                     "Failed to fork: %s", g_strerror (errno));
        return FALSE;
    } else if (data->pid == 0) {
        /* Child process. */
        if (command_data->path && (chdir (command_data->path) == -1)) {
            /* command_path was supplied and we failed to chdir to it. */
            g_warning ("Failed to chdir() to %s: %s\n", command_data->path, g_strerror (errno));
            exit (1);
        }
        environ = (gchar **) command_data->environ;
        /* Spawn the command */
        if (execvp (*command_data->command, (gchar **) command_data->command) == -1) {
            g_warning ("Failed to exec() %s, %s error:%s\n",
                       *command_data->command,
                       command_data->path, g_strerror (errno));
            exit (1);
        }
    }
    /* Parent process. */

    // Localwatchdog handler
    if (command_data->max_time != 0) {
        data->timeout_handler_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                               command_data->max_time,
                                                               process_timeout_callback,
                                                               data,
                                                               NULL);
    }

    // IO handler
    if (io_callback != NULL) {
        GIOChannel *io = g_io_channel_unix_new (data->fd);
        g_io_channel_set_flags (io, G_IO_FLAG_NONBLOCK, NULL);
        data->io_handler_id = g_io_add_watch_full (io,
                                                   G_PRIORITY_DEFAULT,
                                                   G_IO_IN | G_IO_HUP,
                                                   io_callback,
                                                   user_data,
                                                   NULL);
    }
    // Monitor pid for return code
    data->pid_handler_id = g_child_watch_add_full (G_PRIORITY_DEFAULT,
                                                   data->pid,
                                                   process_pid_callback,
                                                   data,
                                                   process_pid_finish);
    return TRUE;
}

void
process_update_localwatchdog (void)
{
}

void
process_pid_callback (GPid pid, gint status, gpointer user_data)
{
    ProcessData *process_data = (ProcessData *) user_data;

    process_data->pid_result = status;
}

void
process_pid_finish (gpointer user_data)
{
    ProcessData *process_data = (ProcessData *) user_data;

    // Remove local watchdog handler
    if (process_data->timeout_handler_id != 0) {
        g_source_remove(process_data->timeout_handler_id);
        process_data->timeout_handler_id = 0;
    }

    process_data->finish_callback (process_data->pid_result,
                                   process_data->localwatchdog,
                                   process_data->user_data);

    // Free process_data.
    process_free (process_data);
}

gboolean
process_timeout_callback (gpointer user_data)
{
    ProcessData *process_data = (ProcessData *) user_data;

    // Kill process pid
    if (kill (process_data->pid, SIGKILL) == 0) {
        process_data->localwatchdog = TRUE;
    } else {
        g_warning("Local watchdog expired! But we failed to kill %i with %i", process_data->pid, SIGKILL);
        // Remove pid handler
        if (process_data->pid_handler_id) {
            g_source_remove(process_data->pid_handler_id);
            process_data->pid_handler_id = 0;
        }
    }
    return FALSE;
}
