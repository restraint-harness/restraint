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

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "task.h"
#include "metadata.h"
#include "process.h"
#include "config.h"
#include "errors.h"
#include "utils.h"

gboolean parse_metadata(Task *task, GError **error) {
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *tmp_error = NULL;

    /* Create a new GKeyFile object and a bitwise list of flags. */
    keyfile = g_key_file_new();
    /* This is not really needed since I don't envision writing the file back out */
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    gchar *task_metadata = g_build_filename(task->path, "metadata", NULL);
    /* Load the GKeyFile from task_metadata or return. */
    if (!g_key_file_load_from_file(keyfile, task_metadata, flags, &tmp_error)) {
        g_propagate_error(error, tmp_error);
        g_free (task_metadata);
        goto error;
    }
    g_free (task_metadata);

    gchar *task_name = g_key_file_get_locale_string(keyfile,
                                                      "General",
                                                      "name",
                                                      task->recipe->osmajor,
                                                      &tmp_error);
    if (tmp_error != NULL) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    if (task_name == NULL)
        g_set_error (error, RESTRAINT_PARSE_ERROR,
                      RESTRAINT_PARSE_ERROR_BAD_SYNTAX, "Missing name");
    task->name = g_strdup(g_strstrip(task_name));
    g_free(task_name);

    gchar *key_entry_point = g_key_file_get_locale_string(keyfile,
                                                      "restraint",
                                                      "entry_point",
                                                      task->recipe->osmajor,
                                                      &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    if (key_entry_point != NULL) {
        g_strfreev (task->entry_point);
        gchar *entry_point = g_strdup_printf ("%s %s", TASK_PLUGIN_SCRIPT, key_entry_point);
        task->entry_point = g_strsplit (entry_point, " ", 0);
        g_free (key_entry_point);
        g_free (entry_point);
    }

    gchar *max_time = g_key_file_get_locale_string (keyfile,
                                                    "restraint",
                                                    "max_time",
                                                    task->recipe->osmajor,
                                                    &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);
    if (max_time != NULL) {
        guint64 time = parse_time_string(max_time, &tmp_error);
        g_free(max_time);
        if (tmp_error) {
            g_propagate_error(error, tmp_error);
            goto error;
        }
        // If max_time is set it's because we read it from our run data
        task->max_time = task->max_time ? task->max_time : time;
    }

    gsize length;
    gchar **dependencies = g_key_file_get_locale_string_list(keyfile,
                                                     "restraint",
                                                     "dependencies",
                                                     task->recipe->osmajor,
                                                     &length,
                                                     &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        g_strfreev(dependencies);
        goto error;
    }
    g_clear_error (&tmp_error);
    for (gsize i=0; i < length; i++) {
        task->dependencies = g_list_prepend(task->dependencies,
                                            g_strdup(g_strstrip(dependencies[i])));
    }
    task->dependencies = g_list_reverse(task->dependencies);
    g_strfreev(dependencies);

    task->nolocalwatchdog = g_key_file_get_boolean (keyfile,
                                                    "restraint",
                                                    "no_localwatchdog",
                                                    &tmp_error);

    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);

    g_key_file_free(keyfile);
    return TRUE;

error:
    g_key_file_free(keyfile);
    return FALSE;
}

gboolean restraint_generate_testinfo(AppData *app_data, GError **error) {
    g_return_val_if_fail(app_data != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    Task *task = app_data->tasks->data;

    TaskRunData *task_run_data = g_slice_new0(TaskRunData);
    task_run_data->app_data = app_data;
    task_run_data->pass_state = TASK_METADATA_PARSE;
    task_run_data->fail_state = TASK_COMPLETE;

    CommandData *command_data = g_slice_new0 (CommandData);
    const gchar *command[] = { "make", "testinfo.desc", NULL };
    command_data->command = command;
    command_data->path = task->path;
    GStatBuf stat_buf;
    GError *tmp_error = NULL;

    gchar *makefile = g_build_filename(task->path, "Makefile", NULL);
    if (g_stat(makefile, &stat_buf) != 0) {
        g_set_error (error, RESTRAINT_PARSE_ERROR,
                     RESTRAINT_MISSING_FILE,
                     "running in rhts_compat mode and missing 'Makefile'");
        g_free(makefile);
        return FALSE;
    }
    g_free(makefile);

    if (!process_run (command_data,
                      task_io_callback,
                      task_handler_callback,
                      task_run_data,
                      &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error,
                                    "While running make testinfo.desc: ");
        return FALSE;
    }

    return TRUE;
}

static void parse_line(Task *task,
                       const gchar *line,
                       gsize length,
                       GError **error) {

    g_return_if_fail(task != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *tmp_error = NULL;
    gchar **key_value = g_strsplit (line,":",2);
    gchar *key = g_ascii_strup(g_strstrip(key_value[0]), -1);
    gchar *value = g_strdup(g_strstrip(key_value[1]));
    g_strfreev(key_value);

    if (g_strcmp0("TESTTIME",key) == 0) {
        guint64 time = parse_time_string(value, &tmp_error);
        if (tmp_error) {
            g_free(key);
            g_free(value);
            g_propagate_error(error, tmp_error);
            return;
        }
        // If max_time is set it's because we read it from our run data
        task->max_time = task->max_time ? task->max_time : time;
    } else if(g_strcmp0("NAME", key) == 0) {
        task->name = g_strdup(g_strstrip(value));
    } else if(g_strcmp0("REQUIRES", key) == 0) {
        gchar **dependencies = g_strsplit(value," ",0);
        GList *list = NULL;
        gint i = 0;
        while (dependencies[i] != NULL) {
            list = g_list_prepend(list, g_strdup(g_strstrip(dependencies[i])));
            i++;
        }
        g_strfreev(dependencies);
        list = g_list_reverse(list);
        if (list) {
            task->dependencies = g_list_concat(task->dependencies, list);
        }
    }
    g_free(key);
    g_free(value);
}

static void flush_parse_buffer(Task *task, GString *parse_buffer, GError **error) {
    g_return_if_fail(task != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *tmp_error = NULL;
    
    if (parse_buffer->len > 0) {
        parse_line(task, parse_buffer->str, parse_buffer->len, &tmp_error);
        g_string_erase(parse_buffer, 0, -1);
        if (tmp_error) {
            g_propagate_error(error, tmp_error);
            return;
        }
    }
}

static void file_parse_data(Task *task,
                       GString *parse_buffer,
                       const gchar *data,
                       gsize length,
                       GError **error) {
    g_return_if_fail(task != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *parse_error;
    gsize i;

    g_return_if_fail(data != NULL || length == 0);

    parse_error = NULL;
    i = 0;
    while (i < length) {
        if (data[i] == '\n') {
            /* remove old style ascii termintaor */
            if (parse_buffer->len > 0
                && (parse_buffer->str[parse_buffer->len - 1]
                    == '\r'))
                g_string_erase(parse_buffer, parse_buffer->len - 1, 1);
            /* When a newline is encountered flush the parse buffer so that the
             * line can be parsed.  Completely blank lines are skipped.
             */
            if (parse_buffer->len > 0)
                flush_parse_buffer(task, parse_buffer, &parse_error);
            if (parse_error) {
                g_propagate_error(error, parse_error);
                return;
            }
            i++;
        } else {
            const gchar *start_of_line;
            const gchar *end_of_line;
            gsize line_length;

            start_of_line = data + i;
            end_of_line = memchr(start_of_line, '\n', length - i);

            if (end_of_line == NULL)
                end_of_line = data + length;

            line_length = end_of_line - start_of_line;

            g_string_append_len(parse_buffer, start_of_line, line_length);
            i += line_length;
        }
    }
}

static gboolean parse_testinfo_from_fd(Task *task,
                                       gint fd,
                                       GError **error) {
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    GError *tmp_error = NULL;
    gssize bytes_read;
    struct stat stat_buf;
    gchar read_buf[4096];
    GString *parse_buffer = g_string_sized_new(128);

    if (fstat(fd, &stat_buf) < 0) {
        g_set_error_literal(error, G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            g_strerror(errno));
        return FALSE;
    }

    if (!S_ISREG (stat_buf.st_mode)) {
        g_set_error_literal (error, G_FILE_ERROR,
                             RESTRAINT_OPEN,
                             "Not a regular file");
      g_string_free(parse_buffer, TRUE);
      return FALSE;
    }

    do {
        bytes_read = read(fd, read_buf, 4096);

        if (bytes_read == 0) /* End of File */
            break;

        if (bytes_read < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            g_set_error_literal(error, G_FILE_ERROR,
                                g_file_error_from_errno(errno),
                                g_strerror(errno));
            g_string_free(parse_buffer, TRUE);
            return FALSE;
        }
        file_parse_data(task,
                        parse_buffer,
                        read_buf,
                        bytes_read,
                        &tmp_error);
    } while (!tmp_error);

    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        g_string_free(parse_buffer, TRUE);
        return FALSE;
    }

    flush_parse_buffer(task, parse_buffer, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        g_string_free(parse_buffer, TRUE);
        return FALSE;
    }
    g_string_free(parse_buffer, TRUE);
    return TRUE;
}

static gboolean parse_testinfo_from_file(Task *task,
                                         GError **error) {
    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *tmp_error = NULL;
    gint fd;


    gchar *task_testinfo = g_build_filename(task->path, "testinfo.desc", NULL);
    fd = g_open(task_testinfo, O_RDONLY, 0);
    g_free (task_testinfo);

    if (fd == -1) {
        g_set_error_literal(error, G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            g_strerror(errno));
        return FALSE;
    }

    parse_testinfo_from_fd(task, fd, &tmp_error);
    close(fd);

    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        return FALSE;
    }
    return TRUE;
}

gboolean
restraint_is_rhts_compat (Task *task)
{
    GStatBuf stat_buf;
    gchar *task_metadata = g_build_filename(task->path, "metadata", NULL);
    if (g_stat(task_metadata, &stat_buf) == 0) {
        g_free (task_metadata);
        return FALSE;
    } else {
        g_free (task_metadata);
        return TRUE;
    }
}

gboolean
restraint_no_testinfo (Task *task)
{
    GStatBuf stat_buf;
    gchar *task_testinfo = g_build_filename(task->path, "testinfo.desc", NULL);
    if (g_stat(task_testinfo, &stat_buf) != 0) {
        g_free (task_testinfo);
        return TRUE;
    } else {
        g_free (task_testinfo);
        return FALSE;
    }
}

gboolean restraint_metadata_parse(Task *task, GError **error) {
    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    GError *tmp_error = NULL;
    gboolean ret;
    /* metadata can be stored in two ways.
     *
     * If rhts_compat is True then we parse "testinfo.desc", if False
     * then we parse "metadata"
     *
     */

    if (task->rhts_compat) {
        ret = parse_testinfo_from_file(task, &tmp_error);
    } else {
        ret = parse_metadata(task, &tmp_error);
    }
    if (!ret) {
        g_propagate_prefixed_error(error, tmp_error,
                                   "Task %s: ", task->task_id);
        return FALSE;
    }

    // Set to default max time if max_time is 0
    task->max_time = task->max_time ? task->max_time : DEFAULT_MAX_TIME;

    return TRUE;
}
