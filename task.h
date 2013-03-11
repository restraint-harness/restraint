
#ifndef _RESTRAINT_TASK_H
#define _RESTRAINT_TASK_H

#include <glib.h>
#include <libsoup/soup.h>

typedef enum {
    TASK_FETCH_INSTALL_PACKAGE,
    TASK_FETCH_UNPACK,
} TaskFetchMethod;

#define RESTRAINT_TASK_FETCH_ERROR restraint_task_fetch_error()
GQuark restraint_task_fetch_error(void);
typedef enum {
    RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
    RESTRAINT_TASK_FETCH_ERROR_GIT_NAK,
    RESTRAINT_TASK_FETCH_ERROR_GIT_REMOTE_ERROR,
} RestraintTaskFetchError;

// error domain for libarchive's errnos
#define RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR restraint_task_fetch_libarchive_error()
GQuark restraint_task_fetch_libarchive_error(void);

typedef struct {
    /* Beaker ID for this task */
    gchar *task_id;
    /* Base URI for this task in the Beaker harness API */
    SoupURI *task_uri;
    /* Task name, populated from task metadata if Beaker doesn't tell us */
    gchar *name;
    /* Filesystem path where the task is unpacked */
    gchar *path;
    /* How to fetch this task */
    TaskFetchMethod fetch_method;
    /* Where/what to fetch this task from */
    union {
        gchar *package_name; // when TASK_FETCH_INSTALL_PACKAGE
        SoupURI *url; // when TASK_FETCH_UNPACK
    } fetch;
    /* Has this task been started already? */
    gboolean started;
    /* Has this task finished already? */
    gboolean finished;
} Task;

Task *restraint_task_new(void);
gboolean restraint_task_fetch_git(Task *task, GError **error);
void restraint_task_run(Task *task);
void restraint_task_free(Task *task);

#endif
