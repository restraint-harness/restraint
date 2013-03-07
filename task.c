
#include <glib.h>

#include "task.h"
#include "packages.h"

static gboolean restraint_task_fetch(Task *task, GError **error) {
    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *tmp_error = NULL;
    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
            g_critical("XXX IMPLEMENTME");
            return FALSE;
            break;
        case TASK_FETCH_INSTALL_PACKAGE:
            if (!restraint_install_package(task->fetch.package_name, &tmp_error)) {
                g_propagate_error(error, tmp_error);
                return FALSE;
            }
            break;
        default:
            g_return_val_if_reached(FALSE);
    }
    return TRUE;
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    return task;
}

void restraint_task_run(Task *task) {
    GError *error = NULL;
    gboolean fetch_succeeded = restraint_task_fetch(task, &error);
    if (!fetch_succeeded) {
        // abort it
    }
    // do more stuff here
}

void restraint_task_free(Task *task) {
    g_return_if_fail(task != NULL);
    g_free(task->task_id);
    soup_uri_free(task->task_uri);
    g_free(task->name);
    switch (task->fetch_method) {
        case TASK_FETCH_INSTALL_PACKAGE:
            g_free(task->fetch.package_name);
            break;
        case TASK_FETCH_UNPACK:
            g_free(task->fetch.url);
            break;
        default:
            g_return_if_reached();
    }
    g_slice_free(Task, task);
}
