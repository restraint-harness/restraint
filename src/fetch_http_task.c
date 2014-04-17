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

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <libsoup/soup-uri.h>

#include "task.h"
#include "common.h"


typedef struct {
    const SoupURI *uri;
    GSocketConnection *connection;
    GSocketClient *client;
    GInputStream *istream;
    GOutputStream *ostream;
} GitArchData;

static ssize_t myread(struct archive *a, void *client_data, const void **abuf) {
    gsize len = 0;

    GError *error = NULL;
    gboolean read_succeeded = TRUE;
    if (!read_succeeded) {
        archive_set_error(a, error->code, "%s", error->message);
        return -1;
    }

    return len;
}

static gboolean myopen(GitArchData *mydata, GError **error) {
    g_return_val_if_fail(mydata != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    //GError *tmp_error = NULL;

    //mydata->client = g_socket_client_new();

    return TRUE;
}

static int myclose(struct archive *a, void *client_data) {
    GitArchData *mydata = client_data;
    GError * error = NULL;

    g_io_stream_close(G_IO_STREAM (mydata->connection),
                      NULL,
                      &error);
    g_object_unref(mydata->client);
    g_object_unref(mydata->connection);
    if (error != NULL) {
        archive_set_error(a, error->code, "%s", error->message);
        return ARCHIVE_FATAL;
    }
    return ARCHIVE_OK;
}

gboolean restraint_task_fetch_http(AppData *app_data, GError **error) {
    g_return_val_if_fail(app_data != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    Task *task = (Task *) app_data->tasks->data;
    GError *tmp_error = NULL;
    struct archive *a = NULL;
    struct archive *ext = NULL;
    struct archive_entry *entry;
    GString *message = g_string_new (NULL);
    gint r;
    const gchar *basePath = task->path;
    gchar *newPath = NULL;
    GitArchData *mydata = g_slice_new0(GitArchData);
    g_return_val_if_fail(mydata != NULL, FALSE);

    mydata->uri = task->fetch.url;

    a = archive_read_new();
    if (a == NULL) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_read_new failed");
        goto error;
    }
    ext = archive_write_disk_new();
    if (ext == NULL) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_write_disk_new failed");
        goto error;
    }
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    gboolean open_succeeded = myopen(mydata, &tmp_error);
    if (!open_succeeded) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    r = archive_read_open(a, mydata, NULL, myread, myclose);
    if (r != ARCHIVE_OK) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_open failed: %s", archive_error_string(a));
        goto error;
    }
    // FIXME - Change this to an idle handler so we return to the main loop
    for (;;) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK) {
            g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                    "archive_read_next_header failed: %s", archive_error_string(a));
            goto error;
        }

        // Update pathname
        newPath = g_build_filename (basePath, archive_entry_pathname( entry ), NULL);
        archive_entry_set_pathname( entry, newPath );
        g_free(newPath);

        g_string_printf (message, "** Extracting %s\n", archive_entry_pathname(entry));
        connections_write (app_data, message->str, message->len);
        g_string_free (message, TRUE);

        r = archive_read_extract2(a, entry, ext);
        if (r != ARCHIVE_OK) {
            g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                    "archive_read_extract2 failed: %s", archive_error_string(ext));
            goto error;
        }
    }

    r = archive_write_free(ext);
    ext = NULL;
    if (r != ARCHIVE_OK) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_write_free failed");
        goto error;
    }
    r = archive_read_free(a);
    a = NULL;
    if (r != ARCHIVE_OK) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_free failed");
        goto error;
    }
    g_slice_free(GitArchData, mydata);

    return TRUE;

error:
    if (ext != NULL) {
        int free_result = archive_write_free(ext);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_write_disk");
    }
    if (a != NULL) {
        int free_result = archive_read_free(a);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_read");
    }
    if (mydata != NULL)
        g_slice_free(GitArchData, mydata);
    return FALSE;
}
