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
#include <unistd.h>
#include <curl/curl.h>

#include "fetch.h"
#include "fetch_uri.h"

struct curl_data {
    CURLM *curlm;
    int to_ev;
    int running;
};

struct socket_data {
    GIOChannel *ch;
    int ev;
};

static size_t cwrite_callback(char *ptr, size_t size, size_t nmemb,
                              void *userdata)
{
    GMemoryInputStream *stream = (GMemoryInputStream *)userdata;

    g_memory_input_stream_add_data(stream, g_memdup(ptr, size * nmemb),
                                   size * nmemb, g_free);

    return size * nmemb;
}

static ssize_t
myread(struct archive *a, void *client_data, const void **abuf)
{
    FetchData *fetch_data = client_data;
    gsize len = 0;
    *abuf = fetch_data->buf;

    GError *error = NULL;

    len = g_input_stream_read (fetch_data->istream, fetch_data->buf, sizeof (fetch_data->buf),
                               NULL, &error);

    if (error) {
        archive_set_error(fetch_data->a, error->code, "%s", error->message);
        return -1;
    }

    return len;
}

static gboolean
myopen(FetchData *fetch_data, GError **error)
{
    g_return_val_if_fail(fetch_data != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    CURLMcode res;
    struct curl_data *cd = fetch_data->private_data;
    CURLM *curlm = cd->curlm;
    CURL *curl = curl_easy_init();
    gchar *uri = soup_uri_to_string(fetch_data->url, FALSE);

    fetch_data->istream = g_memory_input_stream_new();
    curl_easy_setopt(curl, CURLOPT_URL, uri);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cwrite_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fetch_data->istream);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, fetch_data->curl_error_buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    g_free(uri);

    if (fetch_data->ssl_verify == FALSE) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    }

    res = curl_multi_add_handle(curlm, curl);

    if (res != CURLM_OK) {
        g_set_error(error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, 0,
                "Failed to fetch url: %s", fetch_data->curl_error_buf);
        return FALSE;
    }

    return TRUE;
}

static int
myclose(struct archive *a, void *client_data)
{
    FetchData *fetch_data = client_data;
    GError * error = NULL;

    g_input_stream_close(fetch_data->istream,
                      NULL,
                      &error);
    g_object_unref(fetch_data->istream);
    if (error != NULL) {
        archive_set_error(fetch_data->a, error->code, "%s", error->message);
        return ARCHIVE_FATAL;
    }
    return ARCHIVE_OK;
}

static gboolean
archive_finish_callback (gpointer user_data)
{
    FetchData *fetch_data = (FetchData*)user_data;
    struct curl_data *cd = fetch_data->private_data;
    CURLM *curlm = cd->curlm;
    gint free_result;

    if (fetch_data == NULL) {
        g_warning("%s: fetch_data is NULL", __func__);
        return FALSE;
    }

    if (fetch_data->ext != NULL) {
        free_result = archive_write_free(fetch_data->ext);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_write_disk");
    }
    if (fetch_data->a != NULL) {
        free_result = archive_read_free(fetch_data->a);
        if (free_result != ARCHIVE_OK)
            g_warning("Failed to free archive_read");
    }

    if (fetch_data->finish_callback) {
        fetch_data->finish_callback (fetch_data->error,
                                     fetch_data->match_cnt,
                                     fetch_data->nonmatch_cnt,
                                     fetch_data->user_data);
    } else {
        g_clear_error(&fetch_data->error);
    }

    curl_multi_cleanup(curlm);
    g_free(fetch_data->private_data);
    g_slice_free(FetchData, fetch_data);
    return FALSE;
}
static gboolean
is_entry_prefixed_with_fragment(const gchar *entry_path,
                                const gchar *fragment)
{
   
    gchar *skip_entry_branch = NULL;
   
    if (fragment == NULL)
        return FALSE;

    skip_entry_branch = g_strstr_len(entry_path, -1, "/");
    if (skip_entry_branch == NULL) {
        return FALSE;
    }
    skip_entry_branch++;  // Skip '/'
    if (fragment == NULL ||
            (g_strstr_len(skip_entry_branch, strlen(fragment), fragment) != NULL &&
            !(fragment[strlen(fragment)] != '/' && strlen(skip_entry_branch) ==
                strlen(fragment) + 1))
            ) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
http_archive_read_callback (gpointer user_data)
{
    FetchData *fetch_data = (FetchData *) user_data;

    gint r;
    struct archive_entry *entry;
    gchar *newPath = NULL;

    r = archive_read_next_header(fetch_data->a, &entry);
    if (r == ARCHIVE_EOF) {
        if (fetch_data->match_cnt == 0) {
            gchar *archive_error = archive_error_string(entry->archive);
            if (archive_error == NULL)
                    archive_error = "unknown archive error";
            g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_WARN,
                    "Nothing was extracted from archive: %s", archive_error);
        }
        g_idle_add (archive_finish_callback, fetch_data);
        return FALSE;
    }

    if (r != ARCHIVE_OK) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_next_header failed: %s", archive_error_string(fetch_data->a));
        g_idle_add (archive_finish_callback, fetch_data);
        return FALSE;
    }

    const gchar *fragment = fetch_data->url->fragment;
    const gchar *entry_path = archive_entry_pathname(entry);
    if (fragment == NULL || is_entry_prefixed_with_fragment(entry_path, fragment))
        {
        // Update pathname
        if (fragment != NULL) {
            newPath = g_build_filename(fetch_data->base_path,
                                       g_strstr_len(entry_path, -1, fragment) +
                                       strlen(fragment), NULL);
        } else {
            newPath = g_build_filename(fetch_data->base_path, entry_path, NULL);
        }
        archive_entry_set_pathname( entry, newPath );
        g_free(newPath);

        if (fetch_data->keepchanges == FALSE ||
                access(archive_entry_pathname(entry), F_OK) == -1) {
            r = archive_read_extract2(fetch_data->a, entry, fetch_data->ext);
            if (r != ARCHIVE_OK) {
                g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, r,
                        "archive_read_extract2 failed: %s", archive_error_string(fetch_data->ext));
                g_idle_add (archive_finish_callback, fetch_data);
                return FALSE;
            }
            gchar *strbegin = NULL;
            if (fragment) {
                strbegin = g_strstr_len(archive_entry_pathname(entry), -1,
                                        fragment);
            } else {
                strbegin = (gchar *)archive_entry_pathname(entry) + strlen(fetch_data->base_path);
                if (g_strstr_len(strbegin, 1, "/")) {
                    strbegin += 1;
                }
            }
            if ((fetch_data->archive_entry_callback) && (strbegin)) {
                fetch_data->archive_entry_callback (strbegin,
                                                    fetch_data->user_data);
            }

            fetch_data->match_cnt++;
        }
    } else {
        fetch_data->nonmatch_cnt++;
    }
    return TRUE;
}

static void check_multi_info(FetchData *fetch_data)
{
    struct curl_data *cd = fetch_data->private_data;
    CURLM *curlm = cd->curlm;
    CURLMsg *msg;
    int msgs_left;

    while((msg = curl_multi_info_read(curlm, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;
            curl_multi_remove_handle(curlm, easy);
            curl_easy_cleanup(easy);
        }
    }
}

static gboolean event_cb(GIOChannel *ch, GIOCondition condition, gpointer data)
{
    CURLMcode res;
    FetchData *fetch_data = (FetchData*)data;
    struct curl_data *cd = fetch_data->private_data;
    CURLM *curl = cd->curlm;
    int fd = g_io_channel_unix_get_fd(ch);
    int action = (condition & G_IO_IN ? CURL_CSELECT_IN : 0) |
                 (condition & G_IO_OUT ? CURL_CSELECT_OUT : 0);

    res = curl_multi_socket_action(curl, fd, action, &cd->running);
    if (res != CURLM_OK) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, res,
                    "curl failed");
        g_idle_add (archive_finish_callback, fetch_data);
        return FALSE;
    }

    check_multi_info(fetch_data);
    if (cd->running) {
        return TRUE;
    } else {
        if (cd->to_ev != 0) {
            g_source_remove(cd->to_ev);
            cd->to_ev = 0;
        }
        return FALSE;
    }
}

static int sock_cb(CURL *easy,      /* easy handle */
                   curl_socket_t s, /* socket */
                   int what,        /* describes the socket */
                   void *userp,     /* private callback pointer */
                   void *socketp)   /* private socket pointer */
{
    FetchData *fetch_data = (FetchData*)userp;
    struct curl_data *cd = fetch_data->private_data;
    CURLM *curl = cd->curlm;
    struct socket_data *sd = socketp;
    int action = (what & CURL_POLL_IN ? G_IO_IN : 0) |
                 (what & CURL_POLL_OUT ? G_IO_OUT : 0);

    if (what == CURL_POLL_REMOVE) {
        if (sd == NULL) {
            return 0;
        }
        if (sd->ev != 0) {
            g_source_remove(sd->ev);
            sd->ev = 0;
        }
        g_io_channel_unref(sd->ch);
        g_free(sd);
        curl_multi_assign(curl, s, NULL);
    } else {
        if (sd == NULL) {
            sd = g_new0(struct socket_data, 1);
            sd->ch = g_io_channel_unix_new(s);
            curl_multi_assign(curl, s, sd);
        }
        if (sd->ev != 0) {
            g_source_remove(sd->ev);
            sd->ev = 0;
        }
        sd->ev = g_io_add_watch(sd->ch, action, event_cb, fetch_data);
    }
    return 0;
}

static gboolean timer_cb(gpointer data)
{
    FetchData *fetch_data = (FetchData*)data;
    struct curl_data *cd = fetch_data->private_data;
    int cur_timer = cd->to_ev;
    CURLM *curl = cd->curlm;
    CURLMcode res;
    res = curl_multi_socket_action(curl, CURL_SOCKET_TIMEOUT, 0, &cd->running);
    if (res != CURLM_OK) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, res,
                    "curl failed");
        g_idle_add (archive_finish_callback, fetch_data);
        if (cd->to_ev == cur_timer) {
            cd->to_ev = 0;
        }
        return FALSE;
    }
    check_multi_info(fetch_data);
    if (cd->to_ev == cur_timer) {
        cd->to_ev = 0;
    }
    return FALSE;
}

static int update_timeout_cb(CURLM *multi,    /* multi handle */
                             long timeout_ms, /* see above */
                             void *userp)     /* private callback pointer */
{
    FetchData *fetch_data = (FetchData*)userp;
    struct curl_data *cd = fetch_data->private_data;

    if (timeout_ms >= 0) {
        if (cd->to_ev != 0) {
            g_source_remove(cd->to_ev);
        }
        cd->to_ev = g_timeout_add(timeout_ms, timer_cb, userp);
    } else {
        if (cd->to_ev != 0) {
            g_source_remove(cd->to_ev);
            cd->to_ev = 0;
        }
    }
    return 0;
}

static gboolean start_unpack(gpointer data)
{
    FetchData *fetch_data = data;
    struct curl_data *cd = fetch_data->private_data;

    if (cd->running > 0) {
        return TRUE;
    }

    gint r;
    r = archive_read_open(fetch_data->a, fetch_data, NULL, myread, myclose);
    if (r != ARCHIVE_OK) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, r,
                "archive_read_open failed: %s", archive_error_string(fetch_data->a));
        g_idle_add (archive_finish_callback, fetch_data);
        return FALSE;
    }
    g_idle_add (http_archive_read_callback, fetch_data);
    return FALSE;
}

void
restraint_fetch_uri (SoupURI *url,
                     const gchar *base_path,
                     gboolean keepchanges,
                     gboolean ssl_verify,
                     ArchiveEntryCallback archive_entry_callback,
                     FetchFinishCallback finish_callback,
                     gpointer user_data)
{
    g_return_if_fail(url != NULL);
    g_return_if_fail(base_path != NULL);

    FetchData *fetch_data = g_slice_new0(FetchData);
    fetch_data->archive_entry_callback = archive_entry_callback;
    fetch_data->finish_callback = finish_callback;
    fetch_data->user_data = user_data;
    fetch_data->url = url;
    fetch_data->base_path = base_path;
    fetch_data->match_cnt = 0;
    fetch_data->keepchanges = keepchanges;
    fetch_data->ssl_verify = ssl_verify;

    GError *tmp_error = NULL;

    if (keepchanges == FALSE) {
        rmrf(base_path);
    }

    fetch_data->a = archive_read_new();
    if (fetch_data->a == NULL) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_read_new failed");
        g_idle_add (archive_finish_callback, fetch_data);
        return;
    }

    fetch_data->ext = archive_write_disk_new();
    if (fetch_data->ext == NULL) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, 0,
                "archive_write_disk_new failed");
        g_idle_add (archive_finish_callback, fetch_data);
        return;
    }
    archive_read_support_filter_all(fetch_data->a);
    archive_read_support_format_all(fetch_data->a);

    struct curl_data *cd = g_new0(struct curl_data, 1);
    cd->curlm = curl_multi_init();

    if (cd->curlm == NULL) {
        g_set_error(&fetch_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, 0,
                "failed to init curl");
        g_idle_add (archive_finish_callback, fetch_data);
        return;
    }

    fetch_data->private_data = cd;

    curl_multi_setopt(cd->curlm, CURLMOPT_SOCKETFUNCTION, sock_cb);
    curl_multi_setopt(cd->curlm, CURLMOPT_SOCKETDATA, fetch_data);
    curl_multi_setopt(cd->curlm, CURLMOPT_TIMERFUNCTION, update_timeout_cb);
    curl_multi_setopt(cd->curlm, CURLMOPT_TIMERDATA, fetch_data);

    gboolean open_succeeded = myopen(fetch_data, &tmp_error);
    if (!open_succeeded) {
        g_propagate_error(&fetch_data->error, tmp_error);
        g_idle_add (archive_finish_callback, fetch_data);
        return;
    }

    g_timeout_add(500, start_unpack, fetch_data);
}
