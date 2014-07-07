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


#include "task.h"
#include "packages.h"
#include "process.h"

/* Ideas for the future:
 * abstract away yum specifics
 * call yum code more directly, or use PackageKit, or re-use code from PackageKit
 */

gboolean restraint_install_package(AppData *app_data, const gchar *package_name, GError **error) {
    g_return_val_if_fail(package_name != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    TaskRunData *task_run_data = g_slice_new0(TaskRunData);
    task_run_data->app_data = app_data;
    task_run_data->pass_state = TASK_METADATA;
    task_run_data->fail_state = TASK_COMPLETE;

    CommandData *command_data = g_slice_new0 (CommandData);
    const gchar *command[] = { "yum", "-y", "install", package_name, NULL };
    command_data->command = command;
    GError *tmp_error = NULL;

    if (!process_run (command_data,
                      task_io_callback,
                      task_handler_callback,
                      task_run_data,
                      &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error,
                                    "While installing package %s: ", package_name);
        g_slice_free (TaskRunData, task_run_data);
        return FALSE;
    }

    return TRUE;
}
