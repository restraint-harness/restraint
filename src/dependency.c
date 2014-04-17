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

#include "dependency.h"
#include "task.h"
#include "process.h"

static void dependency_handler (gpointer user_data);

static void
dependency_callback (gint pid_result, gboolean localwatchdog, gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
    AppData *app_data = (AppData *) dependency_data->app_data;
    Task *task = (Task *) app_data->tasks->data;

    if (task->rhts_compat != TRUE && pid_result != 0) {
        // If running in rhts_compat mode we don't check whether a packge installed
        // or not.
        // failed to install or remove a package, report fail and abort task
        g_slice_free (DependencyData, dependency_data);
        task->state = TASK_COMPLETE;
        g_set_error (&task->error, RESTRAINT_TASK_RUNNER_ERROR,
                     RESTRAINT_TASK_RUNNER_RC_ERROR,
                     "Command returned non-zero %i", pid_result);
        app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                    task_handler,
                                                    app_data,
                                                    NULL);
    } else {
        dependency_data->dependencies = g_list_next (dependency_data->dependencies);
        dependency_handler (dependency_data);
    }
}

static void
dependency_handler (gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
    AppData *app_data = (AppData *) dependency_data->app_data;
    Task *task = (Task *) app_data->tasks->data;
    if (dependency_data->dependencies) {
        gchar *package_name = dependency_data->dependencies->data;
        CommandData *command_data = g_slice_new0 (CommandData);
        const gchar *install_command[] = {"yum", "-y", "install", package_name, NULL };
        const gchar *remove_command[] = {"yum", "-y", "remove", &package_name[1], NULL };
        if (g_str_has_prefix (package_name, "-") == TRUE) {
            command_data->command = remove_command;
        } else {
            command_data->command = install_command;
        }

        process_run (command_data,
                     task_io_callback,
                     dependency_callback,
                     dependency_data,
                     NULL);
    } else {
        // no more packages to install/remove
        // move on to next stage.
        g_slice_free (DependencyData, dependency_data);
        task->state = TASK_RUN;
        app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                    task_handler,
                                                    app_data,
                                                    NULL);
    }
}

void
restraint_install_dependencies (AppData *app_data)
{
    Task *task = (Task *) app_data->tasks->data;

    DependencyData *dependency_data = g_slice_new0 (DependencyData);
    dependency_data->app_data = app_data;
    dependency_data->dependencies = task->dependencies;

    dependency_handler (dependency_data);
}
