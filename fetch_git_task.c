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

#define GIT_PORT 9418
#define GIT_BRANCH "master"
#define HDR_LEN_SIZE 4
#define LARGE_PACKET_MAX 65520

typedef struct {
    gchar buf[LARGE_PACKET_MAX];
    const SoupURI *uri;
    GSocketConnection *connection;
    GSocketClient *client;
    GInputStream *istream;
    GOutputStream *ostream;
    AppData *app_data;
    struct archive *a;
    struct archive *ext;
} GitArchData;

static gint
packet_length(const gchar *linelen)
{
    gint n;
    gint len = 0;

    for (n = 0; n < HDR_LEN_SIZE; n++) {
        gint val = g_ascii_xdigit_value(linelen[n]);
        if (val < 0)
            return -1;
        len <<= HDR_LEN_SIZE;
        len += val;
    }
    return len;
}

static gboolean
packet_read_line(GInputStream *istream, gchar *buffer, gsize size,
        gsize *size_out, GError **error)
{
    g_return_val_if_fail(istream != NULL, FALSE);
    g_return_val_if_fail(buffer != NULL, FALSE);
    g_return_val_if_fail(size_out != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gint len;
    gsize bytes_read = 0;
    gchar linelen[HDR_LEN_SIZE] = {0};

    GError *tmp_error = NULL;
    gboolean read_succeeded = g_input_stream_read_all(istream, linelen,
            HDR_LEN_SIZE, &bytes_read, NULL, &tmp_error);
    if (!read_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While reading line length from git daemon: ");
        return FALSE;
    }
    if (bytes_read < HDR_LEN_SIZE) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "The remote end hung up unexpectedly");
        return FALSE;
    }
    len = packet_length(linelen);
    if (len < 0) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "protocol error: bad line length character: %.4s", linelen);
        return FALSE;
    }
    if (!len) {
        *size_out = 0;
        return TRUE;
    }
    len -= 4;
    if (len < 0 || len >= size) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "protocol error: bad line length %d", len);
        return FALSE;
    }
    read_succeeded = g_input_stream_read_all(istream, buffer,
            len, &bytes_read, NULL, &tmp_error);
    if (!read_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While reading line length from git daemon: ");
        return FALSE;
    }
    if (bytes_read < len) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "The remote end hung up unexpectedly");
        return FALSE;
    }
    buffer[len] = 0;
    *size_out = len;
    return TRUE;
}

static gboolean
packet_write(GOutputStream *ostream, GError **error,
        const gchar *fmt, ...)
{
    g_return_val_if_fail(ostream != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    va_list args;
    va_start(args, fmt);
    gchar *buffer = NULL;
    gint n = g_vasprintf(&buffer, fmt, args);
    va_end(args);
    if (n < 0)
        return FALSE;
    n += HDR_LEN_SIZE;

    gchar linelen[HDR_LEN_SIZE + 1] = {0};
    if (g_snprintf(linelen, HDR_LEN_SIZE + 1,
            "%0" G_STRINGIFY(HDR_LEN_SIZE) "x", (guint) n) > HDR_LEN_SIZE) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "protocol error: impossibly long line (%d bytes)", n);
        goto error;
    }
    GError *tmp_error = NULL;
    gsize bytes_written = 0;
    gboolean write_succeeded = g_output_stream_write_all(ostream, linelen,
            HDR_LEN_SIZE, &bytes_written, NULL, &tmp_error);
    if (!write_succeeded) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    write_succeeded = g_output_stream_write_all(ostream, buffer,
            (gsize) n - HDR_LEN_SIZE, &bytes_written, NULL, &tmp_error);
    if (!write_succeeded) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_free(buffer);
    return TRUE;

error:
    if (buffer != NULL)
        g_free(buffer);
    return FALSE;
}

static ssize_t
myread(struct archive *a, void *client_data, const void **abuf)
{
    GitArchData *mydata = client_data;
    gint band;
    gsize len = 0;
    *abuf = mydata->buf + 1;

    GError *error = NULL;
    gboolean read_succeeded = packet_read_line(mydata->istream, mydata->buf,
            LARGE_PACKET_MAX, &len, &error);
    if (!read_succeeded) {
        archive_set_error(mydata->a, error->code, "%s", error->message);
        return -1;
    }
    if (len == 0)
        return 0;
    band = mydata->buf[0] & 0xff;
    len--;
    if (band == 2) {
        archive_set_error(mydata->a, 1, "Error from remote service: %s", mydata->buf + 1);
        return -1;
    } else if (band != 1) {
        archive_set_error(mydata->a, 1, "Received data over unrecognized side-band %d", band);
        return -1;
    }
    return len;
}

static gboolean
myopen(GitArchData *mydata, GError **error)
{
    g_return_val_if_fail(mydata != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gsize len;
    gint offset = 0;
    GError *tmp_error = NULL;

    mydata->client = g_socket_client_new();
    guint port = mydata->uri->port != 0 ? mydata->uri->port : GIT_PORT;
    mydata->connection = g_socket_client_connect_to_host(mydata->client,
                                         mydata->uri->host,
                                         port,
                                         NULL,
                                         &tmp_error);
    if (tmp_error != NULL) {
        g_propagate_prefixed_error(error, tmp_error,
                "While connecting to %s:%u: ", mydata->uri->host, port);
        goto error;
    }

    mydata->istream = g_io_stream_get_input_stream (G_IO_STREAM (mydata->connection));
    mydata->ostream = g_io_stream_get_output_stream (G_IO_STREAM (mydata->connection));

    // git-upload-pack
    // git ls-remote git://git2.engineering.redhat.com/users/bpeck/tests.git
    // git-upload-pack /users/bpeck/tests.git.host=git2.engineering.redhat.com.
    if (g_str_has_prefix (mydata->uri->path + 1, "~"))
        offset = 1;

    gboolean write_succeeded = packet_write(mydata->ostream, &tmp_error,
                 "git-upload-archive %s\0host=%s side-band side-band-64k\0",
                 mydata->uri->path + offset, mydata->uri->host);
    if (!write_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While writing to %s: ", mydata->uri->host);
        goto error;
    }
    write_succeeded = packet_write(mydata->ostream, &tmp_error, "argument %s:%s\0",
                           mydata->uri->query == NULL ? GIT_BRANCH : mydata->uri->query, 
                           mydata->uri->fragment == NULL ? "" : mydata->uri->fragment);
    if (!write_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While writing to %s: ", mydata->uri->host);
        goto error;
    }
    write_succeeded = packet_write(mydata->ostream, &tmp_error, "");
    if (!write_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While writing to %s: ", mydata->uri->host);
        goto error;
    }
    gboolean read_succeeded = packet_read_line(mydata->istream, mydata->buf,
            sizeof(mydata->buf), &len, &tmp_error);
    if (!read_succeeded) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    if (!len) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "Expected ACK/NAK from git daemon, got EOF");
        goto error;
    }
    if (mydata->buf[len-1] == '\n')
        mydata->buf[--len] = 0;
    if (strcmp(mydata->buf, "ACK")) {
        if (len > 5 && g_str_has_prefix(mydata->buf, "NACK ")) {
            g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                    RESTRAINT_TASK_FETCH_ERROR_GIT_NAK,
                    "Refused by remote git daemon: %s", mydata->buf);
            goto error;
        }
        if (len > 4 && g_str_has_prefix(mydata->buf, "ERR ")) {
            g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                    RESTRAINT_TASK_FETCH_ERROR_GIT_REMOTE_ERROR,
                    "Error from remote git daemon: %s", mydata->buf + 4);
            goto error;
        }
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "Expected ACK/NAK from git daemon");
        goto error;
    }

    read_succeeded = packet_read_line(mydata->istream, mydata->buf,
            sizeof(mydata->buf), &len, &tmp_error);
    if (!read_succeeded) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    if (len) {
        g_set_error(error, RESTRAINT_TASK_FETCH_ERROR,
                RESTRAINT_TASK_FETCH_ERROR_GIT_PROTOCOL_ERROR,
                "Expected a flush");
        goto error;
    }

    return TRUE;

error:
    if (mydata->connection != NULL)
        g_object_unref(mydata->connection);
    if (mydata->client != NULL)
        g_object_unref(mydata->client);
    return FALSE;
}

static int
myclose(struct archive *a, void *client_data)
{
    GitArchData *mydata = client_data;
    GError * error = NULL;

    g_io_stream_close(G_IO_STREAM (mydata->connection),
                      NULL,
                      &error);
    g_object_unref(mydata->client);
    g_object_unref(mydata->connection);
    if (error != NULL) {
        archive_set_error(mydata->a, error->code, "%s", error->message);
        return ARCHIVE_FATAL;
    }
    return ARCHIVE_OK;
}

static gboolean
git_archive_read_callback (gpointer user_data)
{
    GitArchData *mydata = (GitArchData *) user_data;
    Task *task = (Task *) mydata->app_data->tasks->data;

    gint r;
    struct archive_entry *entry;
    GString *message = g_string_new (NULL);
    const gchar *basePath = task->path;
    gchar *newPath = NULL;

    r = archive_read_next_header(mydata->a, &entry);
    if (r == ARCHIVE_EOF) {
        task->state = TASK_METADATA;
        return FALSE;
    }

    if (r != ARCHIVE_OK) {
        g_set_error(&task->error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_next_header failed: %s", archive_error_string(mydata->a));
        task->state = TASK_FAIL;
        return FALSE;
    }

    // Update pathname
    newPath = g_build_filename (basePath, archive_entry_pathname( entry ), NULL);
    archive_entry_set_pathname( entry, newPath );
    g_free(newPath);

    g_string_printf (message, "** Extracting %s\n", archive_entry_pathname(entry));
    connections_write (mydata->app_data->connections, message, STREAM_STDERR, 0);
    g_string_free (message, TRUE);

    r = archive_read_extract2(mydata->a, entry, mydata->ext);
    if (r != ARCHIVE_OK) {
        g_set_error(&task->error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_extract2 failed: %s", archive_error_string(mydata->ext));
        task->state = TASK_FAIL;
        return FALSE;
    }
    return TRUE;
}

static void
archive_finish_callback (gpointer user_data)
{
    GitArchData *mydata = (GitArchData *) user_data;
    gint free_result;
    if (mydata->ext != NULL) {
        free_result = archive_write_free(mydata->ext);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_write_disk");
    }
    if (mydata->a != NULL) {
        free_result = archive_read_free(mydata->a);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_read");
    }
    if (mydata != NULL)
        g_slice_free(GitArchData, mydata);
}

gboolean
restraint_task_fetch_git (AppData *app_data, GError **error)
{
    g_return_val_if_fail(app_data != NULL, FALSE);
    g_return_val_if_fail(app_data->tasks != NULL, FALSE);

    Task *task = (Task *) app_data->tasks->data;

    GitArchData *mydata = g_slice_new0(GitArchData);
    mydata->app_data = app_data;

    GError *tmp_error = NULL;
    gint r;

    mydata->uri = task->fetch.url;

    mydata->a = archive_read_new();
    if (mydata->a == NULL) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_read_new failed");
        archive_finish_callback (mydata);
        return FALSE;
    }

    mydata->ext = archive_write_disk_new();
    if (mydata->ext == NULL) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_write_disk_new failed");
        archive_finish_callback (mydata);
        return FALSE;
    }
    archive_read_support_filter_all(mydata->a);
    archive_read_support_format_all(mydata->a);
    gboolean open_succeeded = myopen(mydata, &tmp_error);
    if (!open_succeeded) {
        g_propagate_error(error, tmp_error);
        archive_finish_callback (mydata);
        return FALSE;
    }
    r = archive_read_open(mydata->a, mydata, NULL, myread, myclose);
    if (r != ARCHIVE_OK) {
        g_set_error(error, RESTRAINT_TASK_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_open failed: %s", archive_error_string(mydata->a));
        archive_finish_callback (mydata);
        return FALSE;
    }
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    git_archive_read_callback,
                    mydata,
                    archive_finish_callback);
    return TRUE;
}
