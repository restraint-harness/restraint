
#ifndef _RESTRAINT_TASK_H
#define _RESTRAINT_TASK_H

#include <glib.h>

typedef enum {
    TASK_FETCH_INSTALL_PACKAGE,
    TASK_FETCH_UNPACK,
} TaskFetchMethod;

typedef struct {
    gchar *task_id;
    gchar *name;
    TaskFetchMethod fetch_method;
    union {
        gchar *package_name; // when TASK_FETCH_INSTALL_PACKAGE
        gchar *url; // when TASK_FETCH_UNPACK
    } fetch;
    gboolean started;
    gboolean finished;
} Task;

void restraint_task_free(Task *task);

#endif
