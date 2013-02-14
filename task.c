
#include <glib.h>

#include "task.h"

void restraint_task_free(Task *task) {
    g_return_if_fail(task != NULL);
    g_free(task->task_id);
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
