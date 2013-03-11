
#include <string.h>
#include <glib.h>

#include "task.h"
#include "packages.h"

GQuark restraint_task_fetch_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-error-quark");
}

GQuark restraint_task_fetch_libarchive_error(void) {
    return g_quark_from_static_string("restraint-task-fetch-libarchive-error-quark");
}

static gboolean restraint_task_fetch(Task *task, GError **error) {
    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *tmp_error = NULL;
    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
            if (g_strcmp0(soup_uri_get_scheme(task->fetch.url), "git") == 0) {
                if (!restraint_task_fetch_git(task, &tmp_error)) {
                    g_propagate_error(error, tmp_error);
                    return FALSE;
                }
            } else {
                g_critical("XXX IMPLEMENTME");
                return FALSE;
            }
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

static void restraint_task_abort(Task *task, GError *reason) {
    g_return_if_fail(task != NULL);

    // XXX reuse SoupSession, use async version tied to main loop
    SoupSession *soup_session = soup_session_sync_new();

    SoupURI *task_status_uri = soup_uri_new_with_base(task->task_uri, "status");
    SoupMessage *msg = soup_message_new_from_uri("POST", task_status_uri);
    soup_uri_free(task_status_uri);
    g_return_if_fail(msg != NULL);

    gchar *data = NULL;
    if (reason == NULL) {
        // this is basically a bug, but to be nice let's handle it
        g_warning("Aborting task with no reason given");
        data = soup_form_encode("status", "Aborted", NULL);
    } else {
        data = soup_form_encode("status", "Aborted",
                "message", reason->message, NULL);
        g_message("Aborting task %s due to error: %s", task->task_id, reason->message);
    }
    soup_message_set_request(msg, "application/x-www-form-urlencoded",
            SOUP_MEMORY_TAKE, data, strlen(data));

    guint status = soup_session_send_message(soup_session, msg);
    if (!SOUP_STATUS_IS_SUCCESSFUL(status)) {
        g_warning("Failed to abort task %s: libsoup status %u", task->task_id, status);
        // not much else we can do here...
    }

    g_object_unref(soup_session);
}

Task *restraint_task_new(void) {
    Task *task = g_slice_new0(Task);
    return task;
}

void restraint_task_run(Task *task) {
    GError *error = NULL;
    gboolean fetch_succeeded = restraint_task_fetch(task, &error);
    if (!fetch_succeeded) {
        restraint_task_abort(task, error);
        g_error_free(error);
        return;
    }
    // do more stuff here
}

void restraint_task_free(Task *task) {
    g_return_if_fail(task != NULL);
    g_free(task->task_id);
    soup_uri_free(task->task_uri);
    g_free(task->name);
    g_free(task->path);
    switch (task->fetch_method) {
        case TASK_FETCH_INSTALL_PACKAGE:
            g_free(task->fetch.package_name);
            break;
        case TASK_FETCH_UNPACK:
            soup_uri_free(task->fetch.url);
            break;
        default:
            g_return_if_reached();
    }
    g_list_free_full(task->params, (GDestroyNotify) restraint_task_param_free);
    g_slice_free(Task, task);
}

TaskParam *restraint_task_param_new(void) {
    return g_slice_new0(TaskParam);
}

void restraint_task_param_free(TaskParam *task_param) {
    if (task_param->name != NULL)
        g_free(task_param->name);
    if (task_param->value != NULL)
        g_free(task_param->value);
    g_slice_free(TaskParam, task_param);
}
