#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "task.h"
#include "metadata.h"

GQuark restraint_metadata_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-metadata-parse-error-quark");
}

#define unrecognised(error_code, message, ...) g_set_error(error, RESTRAINT_METADATA_PARSE_ERROR, \
        error_code, \
        message, ##__VA_ARGS__)

static guint64 parse_time_string(gchar *time_string, GError **error) {
    /* Convert time string to number of seconds.
     *     5d -> 432000
     *     3m -> 180
     *     2h -> 7200
     *     600s -> 600
     */
    gchar time_unit;
    guint64 max_time = 0;
    sscanf(time_string, "%lu%c", &max_time, &time_unit);
    time_unit = g_ascii_toupper(time_unit);
    if (time_unit == 'D')
        max_time = 24 * 3600 * max_time;
    else if (time_unit == 'H')
        max_time = 3600 * max_time;
    else if (time_unit == 'M')
        max_time = 60 * max_time;
    else if (time_unit == 'S')
        max_time = max_time;
    else {
        unrecognised(RESTRAINT_METADATA_PARSE_ERROR_BAD_SYNTAX, "Unrecognised time unit '%c'", time_unit);
    }
    return max_time;
}

gboolean parse_metadata(Task *task, gchar *task_metadata, GError **error) {
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *tmp_error = NULL;

    /* Create a new GKeyFile object and a bitwise list of flags. */
    keyfile = g_key_file_new();
    /* This is not really needed since I don't envision writing the file back out */
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    /* Load the GKeyFile from task_metadata or return. */
    if (!g_key_file_load_from_file(keyfile, task_metadata, flags, &tmp_error)) {
        g_propagate_error(error, tmp_error);
        goto error;
    }

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
        unrecognised(RESTRAINT_METADATA_PARSE_ERROR_BAD_SYNTAX, "Missing name");
    task->name = g_strdup(g_strstrip(task_name));
    g_free(task_name);

    gchar *entry_point = g_key_file_get_locale_string(keyfile,
                                                      "restraint",
                                                      "entry_point",
                                                      task->recipe->osmajor,
                                                      &tmp_error);
    if (tmp_error != NULL) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    if (entry_point != NULL)
        task->entry_point = g_strsplit(entry_point, " ", 0);
    g_free(entry_point);

    gchar *max_time = g_key_file_get_locale_string (keyfile,
                                                    "restraint",
                                                    "max_time",
                                                    task->recipe->osmajor,
                                                    &tmp_error);
    if (tmp_error != NULL) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
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
    if (tmp_error != NULL) {
        g_propagate_error(error, tmp_error);
        g_strfreev(dependencies);
        goto error;
    }
    for (gsize i=0; i < length; i++) {
        task->dependencies = g_list_prepend(task->dependencies,
                                            g_strdup(g_strstrip(dependencies[i])));
    }
    task->dependencies = g_list_reverse(task->dependencies);
    g_strfreev(dependencies);
    g_key_file_free(keyfile);
    return TRUE;

error:
    g_key_file_free(keyfile);
    return FALSE;
}

gboolean generate_testinfo(gchar *task_path, GError **error) {
    g_return_val_if_fail(task_path != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    const gchar *argv[] = { "make", "testinfo.desc", NULL };
    gchar *make_stdout = NULL;
    gchar *make_stderr = NULL;
    gint make_exit_status = -1;
    GStatBuf stat_buf;
    GError *tmp_error = NULL;

    gchar *makefile = g_build_filename(task_path, "Makefile", NULL);
    if (g_stat(makefile, &stat_buf) != 0) {
        unrecognised(RESTRAINT_METADATA_MISSING_FILE, "running in rhts_compat mode and missing 'Makefile'");
        g_free(makefile);
        return FALSE;
    }
    g_free(makefile);

    gboolean spawn_succeeded = g_spawn_sync(
             task_path,
             (gchar **) argv,
             /* envp */ NULL,
             G_SPAWN_SEARCH_PATH,
             /* child setup closure */ NULL, NULL,
             &make_stdout,
             &make_stderr,
             &make_exit_status,
             &tmp_error);
    if (!spawn_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
             "While running make testinfo.desc: ");
        return FALSE;
    }
    if (!g_spawn_check_exit_status(make_exit_status, &tmp_error)) {
        g_propagate_prefixed_error(error, tmp_error,
             "While running make testinfo.desc: ");
        // XXX include make's stderr in the error message
        g_free(make_stdout);
        g_free(make_stderr);
        return FALSE;
    }
    g_free(make_stdout);
    g_free(make_stderr);
    return TRUE;
}

static void parse_line(Task *task,
                       const gchar *line,
                       gsize length,
                       GError **error) {

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
    } else if(g_strcmp0("REQUIRES", key) == 0 ||
               g_strcmp0("RHTSREQUIRES", key) == 0) {
        gchar **dependencies = g_strsplit(value,",",0);
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
    GError *tmp_error;

    g_return_if_fail(task != NULL);
    
    tmp_error = NULL;
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
                             RESTRAINT_METADATA_OPEN,
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
                                         const gchar *task_testinfo,
                                         GError **error) {
    GError *tmp_error = NULL;
    gint fd;

    g_return_val_if_fail(task != NULL, FALSE);
    g_return_val_if_fail(task_testinfo != NULL, FALSE);

    fd = g_open(task_testinfo, O_RDONLY, 0);

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

void
restraint_parse_run_metadata (Task *task, GError **error)
{
   /*
    * Metadata about the running task
    * rebootcount : how many times this task has rebooted
    * offset      : number of bytes uploaded to lab controller
    * started     : True if the task has started
    * finished    : True if the task has finished
    * max_time    : If we reboot this will be used instead of max_time.
    */

    gchar *run_metadata = g_build_filename(task->run_path, "metadata", NULL);

    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *tmp_error = NULL;

    keyfile = g_key_file_new ();
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    if (!g_key_file_load_from_file (keyfile, run_metadata, flags, NULL)) {
        goto error;
    }

    task->max_time = g_key_file_get_uint64 (keyfile,
                                           "restraint",
                                           "max_time",
                                           &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->reboots = g_key_file_get_uint64 (keyfile,
                                           "restraint",
                                           "reboots",
                                           &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->offset = g_key_file_get_uint64 (keyfile,
                                          "restraint",
                                          "offset",
                                          &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->started = g_key_file_get_boolean (keyfile,
                                          "restraint",
                                          "started",
                                          &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->finished = g_key_file_get_boolean (keyfile,
                                          "restraint",
                                          "finished",
                                          &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

    task->name = g_key_file_get_string (keyfile,
                                          "restraint",
                                          "name",
                                          &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_prefixed_error(error, tmp_error,
                    "Task %s:  parse_run_metadata,", task->task_id);
        goto error;
    }
    g_clear_error (&tmp_error);

error:
    g_key_file_free (keyfile);   
    g_free(run_metadata);
    task->parsed = TRUE;
}

void
restraint_set_run_metadata (Task *task, gchar *key, GError **error, GType type, ...)
{
    va_list args;
    GValue value;

    va_start (args, type);
    SOUP_VALUE_SETV (&value, type, args);
    va_end (args);

    gchar *run_metadata = g_build_filename(task->run_path, "metadata", NULL);
    gchar *s_data = NULL;
    gsize length;
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *tmp_error = NULL;

    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    g_mkdir_with_parents (task->run_path, 0755 /* drwxr-xr-x */);
    keyfile = g_key_file_new ();
    g_key_file_load_from_file (keyfile, run_metadata, flags, NULL);

    switch (type) {
        case G_TYPE_UINT64:
            g_key_file_set_uint64 (keyfile,
                                   "restraint",
                                   key,
                                   g_value_get_uint64 (&value));
            break;
        case G_TYPE_BOOLEAN:
            g_key_file_set_boolean (keyfile,
                                   "restraint",
                                   key,
                                   g_value_get_boolean (&value));
            break;
        case G_TYPE_STRING:
            g_key_file_set_string (keyfile,
                                   "restraint",
                                   key,
                                   g_value_get_string (&value));
            break;
        default:
            g_warning ("invalid GType\n");
    }

    s_data = g_key_file_to_data (keyfile, &length, &tmp_error);
    if (!s_data) {
        g_propagate_error (error, tmp_error);
        goto error;
    }

    if (!g_file_set_contents (run_metadata, s_data, length,  &tmp_error)) {
        g_propagate_error (error, tmp_error);
        goto error;
    }
    
error:
    g_free (s_data);
    g_key_file_free (keyfile);   
    g_free (run_metadata);
}

gboolean restraint_metadata_update(Task *task, GError **error) {
    GError *tmp_error = NULL;
    GStatBuf stat_buf;
    /* metadata can be stored in two ways.
     *
     * First We look for the file "metadata"
     *
     * if metadata doesn't exist we attempt to generate "testinfo.desc" by issuing
     *  make testinfo.desc in the tasks directory.  This also means we are running in
     *  rhts_compat mode.
     */
    gchar *task_metadata = g_build_filename(task->path, "metadata", NULL);
    gchar *task_testinfo = g_build_filename(task->path, "testinfo.desc", NULL);

    if (g_stat(task_metadata, &stat_buf) == 0) {
        task->rhts_compat = FALSE;
        if (!parse_metadata(task, task_metadata, &tmp_error)) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s: ", task->task_id);
            g_free(task_metadata);
            g_free(task_testinfo);
            return FALSE;
        }
    } else {
        task->rhts_compat = TRUE;
        if (!generate_testinfo(task->path, &tmp_error)) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s: ", task->task_id);
            g_free(task_testinfo);
            g_free(task_metadata);
            return FALSE;
        }
        if (!parse_testinfo_from_file(task, task_testinfo, &tmp_error)) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s: ", task->task_id);
            g_free(task_testinfo);
            g_free(task_metadata);
            return FALSE;
        }
    }

    // Set to default max time if max_time is 0
    task->max_time = task->max_time ? task->max_time : DEFAULT_MAX_TIME;

    g_free(task_metadata);
    g_free(task_testinfo);
    return TRUE;
}
