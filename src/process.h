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

#include <gio/gio.h>

#define HEARTBEAT 1 * 60 // heartbeat every 1 minute

/* Used for process IO callbacks */
#define IO_BUFFER_SIZE 8192

typedef void (*ProcessTimeoutCallback) (gpointer user_data,
                                        guint64 *time_remain);

typedef void (*ProcessFinishCallback)   (gint           pid_result,
                                         gboolean       localwatchdog,
                                         gpointer       user_data,
                                         GError         *error);

#define RESTRAINT_PROCESS_ERROR restraint_process_error()
GQuark restraint_process_error(void);

typedef enum {
    RESTRAINT_PROCESS_FORK_ERROR,
} RestraintProcessError;

typedef struct {
    // Command to run
    gchar **command;
    // Environment to use
    const gchar **environ;
    // The path to chdir before executing
    const gchar *path;
    guint64 max_time;
    // pid of our forked process
    pid_t pid;
    // file descriptors of our pty
    gint fd_out;
    gint fd_in;
    // return code result from command
    gint pid_result;
    // id of the io handler
    guint io_handler_id;
    // IO channel
    GIOChannel *io;
    // id of the pid handler
    guint pid_handler_id;
    // id of finish handler
    guint finish_handler_id;
    // id of the timeout handler
    guint timeout_handler_id;
    // True if localwatch kicked in for this process
    gboolean localwatchdog;
    // IO handler to call
    ProcessTimeoutCallback timeout_callback;
    GIOFunc io_callback;
    // next handler to call.
    ProcessFinishCallback finish_callback;
    gpointer user_data;
    GError *error;
    GCancellable *cancellable;
    gulong cancel_handler;
} ProcessData;

void
process_run (const gchar *command,
                      const gchar **environ,
                      const gchar *path,
                      gboolean use_pty,
                      guint64 max_time,
                      ProcessTimeoutCallback timeout_callback,
                      GIOFunc io_callback,
                      ProcessFinishCallback finish_callback,
                      const gchar *content_input,
                      gssize content_size,
                      gboolean buffer,
                      GCancellable *cancellable,
                      gpointer user_data);
//gboolean process_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data);
void process_pid_callback (GPid pid, gint status, gpointer user_data);
gboolean process_pid_finish (gpointer user_data);
gboolean process_timeout_callback (gpointer user_data);
//gboolean process_heartbeat_callback (gpointer user_data);
void process_free (ProcessData *process_data);

extern char **environ;
int    kill(pid_t, int);
