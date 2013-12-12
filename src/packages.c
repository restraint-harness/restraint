
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
                      task_finish_callback,
                      task_run_data,
                      &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error,
                                    "While installing package %s: ", package_name);
        return FALSE;
    }

    return TRUE;
}
