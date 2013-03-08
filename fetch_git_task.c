#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <libsoup/soup-uri.h>

#include "task.h"

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
} GitArchData;

#define hex(a) (hexchar[(a) & 15])
static gchar buffer[1000];

gssize safe_stream_write(GOutputStream *ostream, const void *buf, gssize n) {
    GError * error = NULL;
    gssize nn = n;
    while (n) {
        gint ret = g_output_stream_write (ostream, buf, n, NULL, &error);
        if (ret > 0) {
            buf = (gchar *) buf + ret;
            n -= ret;
            continue;
        }
        if (!ret)
            g_print(error->message);
        g_print("write error");
    }
    return nn;
}

gssize read_in_full(GInputStream *istream, void *buf, gsize count) {
    gchar *p = buf;
    gssize total = 0;
    GError * error = NULL;

    while (count > 0) {
        gssize loaded = g_input_stream_read (istream, p, count, NULL, &error);
        if (loaded < 0) {
            g_print(error->message);
            return -1;
        }
        if (loaded == 0)
            return total;
        count -= loaded;
        p += loaded;
        total += loaded;
    }
    return total;
}

static void safe_read(GInputStream *istream, void *buffer, unsigned size) {
    gssize ret = read_in_full(istream, buffer, size);
    if (ret < 0)
        g_print("read error");
    else if (ret < size)
        g_print("The remote end hung up unexpectedly");
}

static gint packet_length(const gchar *linelen) {
    gint n;
    gint len = 0;

    for (n = 0; n < HDR_LEN_SIZE; n++) {
        unsigned char c = linelen[n];
        len <<= HDR_LEN_SIZE;
        if (c >= '0' && c <= '9') {
            len += c - '0';
            continue;
        }
        if (c >= 'a' && c <= 'f') {
            len += c - 'a' + 10;
            continue;
        }
        if (c >= 'A' && c <= 'F') {
            len += c - 'A' + 10;
            continue;
        }
        return -1;
    }
    return len;
}

gint packet_read_line(GInputStream *istream, gchar *buffer, unsigned size) {
    gint len;
    gchar linelen[HDR_LEN_SIZE];

    safe_read(istream, linelen, HDR_LEN_SIZE);
    len = packet_length(linelen);
    if (len < 0) {
        g_print("protocol error: bad line length character: %.4s", linelen);
        return -1;
    }
    if (!len)
        return 0;
    len -= 4;
    if (len >= size) {
        g_print("protocol error: bad line length %d", len);
        return -1;
    }
    safe_read(istream, buffer, len);
    buffer[len] = 0;
    return len;
}

static unsigned format_packet(const gchar *fmt, va_list args) {
    static gchar hexchar[] = "0123456789abcdef";
    unsigned n;

    n = vsnprintf(buffer + HDR_LEN_SIZE, sizeof(buffer) - HDR_LEN_SIZE, fmt, args);
    if (n >= sizeof(buffer)-HDR_LEN_SIZE) {
        g_print("protocol error: impossibly long line");
        return -1;
    }
    n += HDR_LEN_SIZE;
    buffer[0] = hex(n >> 12);
    buffer[1] = hex(n >> 8);
    buffer[2] = hex(n >> 4);
    buffer[3] = hex(n);
    return n;
}

void packet_write(GOutputStream *ostream, const gchar *fmt, ...) {
    va_list args;
    unsigned n;

    va_start(args, fmt);
    n = format_packet(fmt, args);
    va_end(args);
    safe_stream_write(ostream, buffer, n);
}

void packet_flush(GOutputStream *ostream) {
    safe_stream_write(ostream, "0000", 4);
}

gint prefixcmp(const gchar *str, const gchar *prefix) {
    for (; ; str++, prefix++)
        if (!*prefix)
            return 0;
        else if (*str != *prefix)
            return (unsigned char)*prefix - (unsigned char)*str;
}

ssize_t myread(struct archive *a, void *client_data, const void **abuf) {
    GitArchData *mydata = client_data;
    gint band, len;
    *abuf = mydata->buf + 1;

    len = packet_read_line(mydata->istream, mydata->buf, LARGE_PACKET_MAX);
    if (len == 0)
        return 0;
    if (len < 1)
        return -1;
    band = mydata->buf[0] & 0xff;
    len--;
    if (band != 1)
        return -1;
    return len;
}

int myopen(struct archive *a, void *client_data) {
    GitArchData *mydata = client_data;
    gint len;
    GError * error = NULL;

    mydata->client = g_socket_client_new();
    mydata->connection = g_socket_client_connect_to_host(mydata->client,
                                         mydata->uri->host,
                                         mydata->uri->port == 0 ? GIT_PORT : mydata->uri->port,
                                         NULL,
                                         &error);
    if (error != NULL) {
        g_print(error->message);
        return ARCHIVE_FATAL;
    }

    mydata->istream = g_io_stream_get_input_stream (G_IO_STREAM (mydata->connection));
    mydata->ostream = g_io_stream_get_output_stream (G_IO_STREAM (mydata->connection));

    packet_write(mydata->ostream, "git-upload-archive %s\0host=%s side-band side-band-64k\0",
                 mydata->uri->path, mydata->uri->host);
    packet_write(mydata->ostream, "argument %s:%s\0", 
                           mydata->uri->query == NULL ? GIT_BRANCH : mydata->uri->query, 
                           mydata->uri->fragment == NULL ? "" : mydata->uri->fragment);
    packet_flush(mydata->ostream);
    len = packet_read_line(mydata->istream, mydata->buf, sizeof(mydata->buf));
    if (!len) {
        g_print("git archive: expected ACK/NAK, got EOF");
        return ARCHIVE_FATAL;
    }
    if (mydata->buf[len-1] == '\n')
        mydata->buf[--len] = 0;
    if (strcmp(mydata->buf, "ACK")) {
        if (len > 5 && !prefixcmp(mydata->buf, "NACK ")) {
            g_print("git archive: NACK %s", mydata->buf + 5);
            return ARCHIVE_FATAL;
        }
        if (len > 4 && !prefixcmp(mydata->buf, "ERR ")) {
            g_print("remote error: %s", mydata->buf + 4);
            return ARCHIVE_FATAL;
        }
        g_print("git archive: protocol error");
        return ARCHIVE_FATAL;
    }

    len = packet_read_line(mydata->istream, mydata->buf, sizeof(mydata->buf));
    if (len) {
        g_print("git archive: expected a flush");
        return ARCHIVE_FATAL;
    }

    return ARCHIVE_OK;
}

int myclose(struct archive *a, void *client_data) {
    GitArchData *mydata = client_data;
    GError * error = NULL;

    g_io_stream_close(G_IO_STREAM (mydata->connection),
                      NULL,
                      &error);
    if (error != NULL) {
        g_print(error->message);
        return ARCHIVE_FATAL;
    }
    g_object_unref(mydata->client);
    g_object_unref(mydata->connection);
    return ARCHIVE_OK;
}

static gint copy_data(struct archive *ar, struct archive *aw) {
    gint r;
    const void *buff;
    gsize size;
#if ARCHIVE_VERSION >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return (ARCHIVE_OK);
        if (r != ARCHIVE_OK)
            return (r);
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            g_print("archive_write_data_block():%s", archive_error_string(aw));
            return (r);
        }
    }
}

gboolean restraint_task_fetch_git(Task *task) {
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    gint r;
    const gchar *basePath = task->path;
    gchar *newPath = NULL;
    GitArchData *mydata = g_slice_new(GitArchData);

    mydata->uri = task->fetch.url;

/*
#define TASK_LOCATION "./tmp"
    basePath = g_build_filename(TASK_LOCATION, 
                            mydata->uri->host,
                            mydata->uri->path,
                            mydata->uri->fragment,
                            NULL);
*/

    a = archive_read_new();
    ext = archive_write_disk_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    archive_read_open(a, mydata, myopen, myread, myclose);
    for (;;) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK) {
            g_print("archive read_next_header():%s", archive_error_string(a));
            return FALSE;
        }
        // Update pathname
        newPath = g_build_filename (basePath, archive_entry_pathname( entry ), NULL);
        archive_entry_set_pathname( entry, newPath );

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            g_print("archive_write_header():%s", archive_error_string(ext));
            return FALSE;
        } else {
            copy_data(a, ext);
            r = archive_write_finish_entry(ext);
            if (r != ARCHIVE_OK) {
                g_print("archive_write_finish_entry():%s", archive_error_string(ext));
                return FALSE;
            }
        }
        g_print("%s\n", archive_entry_pathname(entry));
        free(newPath);
    }
    archive_read_close(a);
    archive_read_free(a);

    return TRUE;
}
