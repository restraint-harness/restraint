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

#define _XOPEN_SOURCE 500

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include "client.h"
#include "multipart.h"
#include "errors.h"
#include "xml.h"

static SoupSession *session;

static void recipe_finish(RecipeData *recipe_data);
static gboolean run_recipe_handler (gpointer user_data);

static void restraint_free_recipe_data(RecipeData *recipe_data)
{
    if (recipe_data->tasks != NULL) {
        g_hash_table_destroy(recipe_data->tasks);
    }
    soup_uri_free(recipe_data->remote_uri);
    g_string_free(recipe_data->body, TRUE);
    g_slice_free(RecipeData, recipe_data);
}

void clear_regex (gpointer user_data)
{
    RegexData *regex_data = (RegexData *) user_data;
    regfree (&regex_data->regex);
    g_slice_free (RegexData, regex_data);
}

static void restraint_free_app_data(AppData *app_data)
{
    g_clear_error(&app_data->error);
    g_free(app_data->run_dir);

    if (app_data->result_states_to != NULL) {
        g_hash_table_destroy(app_data->result_states_to);
    }
    g_hash_table_destroy(app_data->recipes);
    if (app_data->loop != NULL) {
        g_main_loop_unref(app_data->loop);
    }

    // Free the regexes
    g_slist_free_full (app_data->regexes, clear_regex);

    g_slice_free(AppData, app_data);
}

static void
update_chunk (gchar *filename, const gchar *data, gsize size, goffset offset)
{
    gint fd = g_open(filename, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        g_warning("Failed to open %s: %s", filename, strerror(errno));
        return;
    }

    if (lseek (fd, offset, SEEK_SET) < 0) {
        g_warning("Failed to seek %s to %" PRId64 ": %s", filename, offset,
                   strerror(errno));
    } else {
        ssize_t written = write (fd, data, size);
        g_warn_if_fail(written == size);
    }

    if (g_close(fd, NULL) < 0) {
        g_warning("Failed to close %s: %s", filename, strerror(errno));
    }
}

static void
init_result_hash (AppData *app_data)
{
    static gint none = 0;
    static gint pass = 1;
    static gint warn = 2;
    static gint fail = 3;

    GHashTable *result_hash_to = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(result_hash_to, "NONE", &none);
    g_hash_table_insert(result_hash_to, "PASS", &pass);
    g_hash_table_insert(result_hash_to, "WARN", &warn);
    g_hash_table_insert(result_hash_to, "FAIL", &fail);

    app_data->result_states_to = result_hash_to;
}

static gboolean
tasks_finished (xmlDocPtr doc, xmlNodePtr node, xmlChar *path)
{
    gboolean finished = TRUE;

    xmlXPathObjectPtr task_nodes = get_node_set (doc, node, path);
    for (guint i = 0; i < task_nodes->nodesetval->nodeNr; i++) {
        xmlNodePtr task_node = task_nodes->nodesetval->nodeTab[i];
        gchar *status = (gchar *)xmlGetNoNsProp(task_node,
                                                (xmlChar *)"status");
        finished &= (strcmp (status, "Completed") == 0 || strcmp(status,
                     "Aborted") == 0);
        g_free (status);
    }
    xmlXPathFreeObject (task_nodes);
    return finished;
}

gboolean
quit_loop_handler (gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit (loop);
    return FALSE;
}

void
record_log (xmlNodePtr node_ptr,
            const gchar *path,
            const gchar *filename)
{
    xmlNodePtr log_node_ptr = xmlNewTextChild (node_ptr,
                                               NULL,
                                               (xmlChar *) "log",
                                               NULL);
    xmlSetProp (log_node_ptr, (xmlChar *)"path", (xmlChar *) path);
    xmlSetProp (log_node_ptr, (xmlChar *)"filename", (xmlChar *) filename);
}

static void
record_result (xmlNodePtr results_node_ptr,
               const gchar *result_id,
               const gchar *result,
               const gchar *message,
               const gchar *path,
               const gchar *score,
               gint verbose)
{
    // record result under task_node_ptr
    xmlNodePtr result_node_ptr = xmlNewTextChild (results_node_ptr,
                                                  NULL,
                                                  (xmlChar *) "result",
                                                  (xmlChar *) message);
    xmlSetProp (result_node_ptr, (xmlChar *)"id", (xmlChar *) result_id);
    xmlSetProp (result_node_ptr, (xmlChar *)"path", (xmlChar *) path);
    xmlSetProp (result_node_ptr, (xmlChar *)"result", (xmlChar *) result);

    // add a logs node
    xmlNewTextChild (result_node_ptr,
                     NULL,
                     (xmlChar *) "logs",
                     NULL);

    if (score)
        xmlSetProp (result_node_ptr, (xmlChar *)"score", (xmlChar *) score);

    if (verbose == 1) {
        // FIXME - read the terminal width and base this value off that.
        gint offset = (gint) strlen (path) - 48;
        const gchar *offset_path = NULL;
        if (offset < 0) {
            offset_path = path;
        } else {
            offset_path = &path[offset];
        }
        g_print ("**   %4s [%-48s] %s", result_id,
                 offset_path, result);
        if (score != NULL) {
            g_print (" Score: %s", score);
        }
        g_print ("\n");
        if (message) {
            g_print ("**            %s\n", message);
        }
    }
}

static gint
result_to_id (gchar *result, GHashTable *states)
{
    gint id = 0;
    gpointer val = g_hash_table_lookup (states, result);
    if (val)
        id = *((gint *)val);
    return id;
}

static void
put_doc (xmlDocPtr xml_doc, gchar *filename)
{
    FILE *outxml = fopen(filename, "w");
    if (outxml == NULL) {
        g_warning("Failed to open %s: %s", filename, strerror(errno));
        return;
    }
    xmlDocFormatDump (outxml, xml_doc, 1);
    fclose (outxml);
}

void
remote_hup (GError *error,
             gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData *) user_data;
    AppData *app_data = recipe_data->app_data;
    gchar *full_url = NULL;
    SoupURI *continue_uri = NULL;

    // If we get an error on the first connection then we simply abort
    if (app_data->started && ! tasks_finished(app_data->xml_doc, recipe_data->recipe_node_ptr, (xmlChar *)"task")
                          && ! g_cancellable_is_cancelled(recipe_data->cancellable)) {
        if (error) {
            g_print ("%s [%s, %d]\n", error->message,
                     g_quark_to_string (error->domain), error->code);
        }
        continue_uri = soup_uri_new_with_base (recipe_data->remote_uri, "/continue");
        soup_uri_free (recipe_data->remote_uri);
        recipe_data->remote_uri = continue_uri;
        full_url = soup_uri_to_string (recipe_data->remote_uri, FALSE);
        g_print ("Disconnected.. delaying %d seconds.\n", DEFAULT_DELAY);
        g_free (full_url);
        // Try and re-connect to the other host
        g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                    DEFAULT_DELAY,
                                    run_recipe_handler,
                                    recipe_data,
                                    NULL);
    } else {
        if (! app_data->error && error) {
            app_data->error = g_error_copy (error);
        }
        recipe_finish (recipe_data);
    }
}

void
tasks_results_cb (const char *method,
                  const char *path,
                  GCancellable *cancellable,
                  GError *error,
                  SoupMessageHeaders *headers,
                  SoupBuffer *body,
                  gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    AppData *app_data = recipe_data->app_data;
    GHashTable *table;
    gchar *task_id = NULL;
    gchar *recipe_id = NULL;
    gchar **entries = NULL;

    const gchar *transaction_id = soup_message_headers_get_one (headers, "transaction-id");
    // Pull some values out of the path.
    entries = g_strsplit (path, "/", 0);
    task_id = g_strdup (entries[4]);
    recipe_id = g_strdup (entries[2]);

    app_data->started = TRUE;

    // Lookup our task
    xmlNodePtr task_node_ptr = g_hash_table_lookup(recipe_data->tasks,
                                                   task_id);
    if (!task_node_ptr) {
        goto cleanup;
    }

    // Record results
    table = soup_form_decode (body->data);
    gchar *result = g_hash_table_lookup (table, "result");
    gchar *message = g_hash_table_lookup (table, "message");
    gchar *result_path = g_hash_table_lookup (table, "path");
    gchar *score = g_hash_table_lookup (table, "score");

    xmlNodePtr results_node_ptr = first_child_with_name(task_node_ptr,
                                                        "results", TRUE);
    // Record the result and get our result id back.
    record_result(results_node_ptr, transaction_id, result, message,
                  result_path, score, app_data->verbose);

    gchar *recipe_result = (gchar *)xmlGetNoNsProp(
        recipe_data->recipe_node_ptr, (xmlChar*)"result");
    gchar *task_result = (gchar *)xmlGetNoNsProp(task_node_ptr,
                                                 (xmlChar*)"result");

    // Push higher priority results up the chain, result->task->recipe.
    if (result_to_id(result, app_data->result_states_to) >
            result_to_id(task_result, app_data->result_states_to))
        xmlSetProp(task_node_ptr, (xmlChar *)"result", (xmlChar *) result);
    if (result_to_id(result, app_data->result_states_to) >
            result_to_id(recipe_result, app_data->result_states_to))
        xmlSetProp (recipe_data->recipe_node_ptr, (xmlChar*)"result",
                    (xmlChar*)result);
    g_free(recipe_result);
    g_free(task_result);

    g_hash_table_destroy(table);
cleanup:
    g_free (task_id);
    g_free (recipe_id);
    g_strfreev (entries);
}

gboolean
watchdog_timeout_cb (gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    AppData *app_data = recipe_data->app_data;
    if (! app_data->error) {
        g_set_error (&app_data->error, RESTRAINT_ERROR,
                     RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
                     "Recipe %d exceeded lab watchdog timer",
                     recipe_data->recipe_id);
    }
    g_cancellable_cancel (recipe_data->cancellable);
    return FALSE;
}

void
watchdog_cb (const char *method,
             const char *path,
             GCancellable *cancellable,
             GError *error,
             SoupMessageHeaders *headers,
             SoupBuffer *body,
             gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    GHashTable *table;

    table = soup_form_decode (body->data);
    gchar *seconds_string = g_hash_table_lookup (table, "seconds");
    guint64 max_time = 0;
    sscanf (seconds_string, "%" SCNu64, &max_time);
    g_hash_table_destroy(table);
    if (recipe_data->timeout_handler_id != 0) {
        g_source_remove (recipe_data->timeout_handler_id);
    }
    recipe_data->timeout_handler_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                                  max_time,
                                                                  watchdog_timeout_cb,
                                                                  recipe_data,
                                                                  NULL);
}

void
tasks_status_cb (const char *method,
                 const char *path,
                 GCancellable *cancellable,
                 GError *error,
                 SoupMessageHeaders *headers,
                 SoupBuffer *body,
                 gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    AppData *app_data = recipe_data->app_data;
    GHashTable *table;
    gchar *task_id = NULL;
    gchar *recipe_id = NULL;
    gchar **entries = NULL;

    const gchar *transaction_id = soup_message_headers_get_one (headers, "transaction-id");
    // Pull some values out of the path.
    entries = g_strsplit (path, "/", 0);
    task_id = g_strdup (entries[4]);
    recipe_id = g_strdup (entries[2]);

    app_data->started = TRUE;

    // Lookup our task
    xmlNodePtr task_node_ptr = g_hash_table_lookup(recipe_data->tasks,
                                                   task_id);
    if (!task_node_ptr) {
        goto cleanup;
    }

    table = soup_form_decode (body->data);
    gchar *status = g_hash_table_lookup (table, "status");
    gchar *message = g_hash_table_lookup (table, "message");
    if (app_data->verbose < 2) {
        xmlChar *task_name = xmlGetNoNsProp(task_node_ptr,
                                            (xmlChar *)"name");
        xmlChar *task_result = xmlGetNoNsProp(task_node_ptr,
                                              (xmlChar *)"result");
        g_print ("*  T: %3s [%-48s] %s",
                 task_id,
                 (gchar *)task_name,
                 status);
        if (g_strcmp0 ("None", (gchar *)task_result) != 0) {
            g_print (": %s", (gchar *)task_result);
        }
        g_print ("\n");
        xmlFree (task_name);
        xmlFree (task_result);
    }
    // If message is passed then record a result with that.
    if (message) {
        xmlNodePtr results_node_ptr = first_child_with_name(task_node_ptr,
                "results", TRUE);
        record_result(results_node_ptr, transaction_id, "Warn", message, "/", NULL,
                      app_data->verbose);
    }

    xmlSetProp (task_node_ptr, (xmlChar *)"status", (xmlChar *) status);
    gchar *recipe_status = (gchar*)xmlGetNoNsProp(
            recipe_data->recipe_node_ptr, (xmlChar*)"status");

    // If recipe status is not already "Aborted" then record push
    // task status to recipe.
    if (g_strcmp0(recipe_status, "Aborted") != 0)
        xmlSetProp(recipe_data->recipe_node_ptr, (xmlChar*)"status",
                   (xmlChar*)status);
    g_free(recipe_status);

    // Write out the current version of the results xml.
    gchar *filename = g_build_filename(app_data->run_dir, "job.xml",
                                       NULL);
    put_doc (app_data->xml_doc, filename);

    g_free(filename);

    g_hash_table_destroy(table);
cleanup:
    g_free (task_id);
    g_free (recipe_id);
    g_strfreev (entries);
}

void
tasks_logs_cb (const char *method,
               const char *path,
               GCancellable *cancellable,
               GError *error,
               SoupMessageHeaders *headers,
               SoupBuffer *body,
               gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    AppData *app_data = recipe_data->app_data;
    GHashTable *table;
    gchar *task_id = NULL;
    gchar *recipe_id = NULL;
    gchar **entries = NULL;

    // Pull some values out of the path.
    entries = g_strsplit (path, "/", 0);
    task_id = g_strdup (entries[4]);
    recipe_id = g_strdup (entries[2]);

    app_data->started = TRUE;

    // Lookup our task
    xmlNodePtr task_node_ptr = g_hash_table_lookup(recipe_data->tasks,
                                                   task_id);
    if (!task_node_ptr) {
        goto cleanup;
    }

    table = soup_form_decode (body->data);
    goffset start;
    goffset end;
    goffset total_length;
    gchar *short_path = NULL;
    gchar *log_path = g_strjoinv ("/", &entries[1]);
    gchar *filename = g_strdup_printf("%s/%s", app_data->run_dir,
                                      log_path);

    gchar *logs_xpath = NULL;
    if (g_strcmp0 (entries[5], "logs") == 0) {
        logs_xpath = g_strdup_printf("task[@id='%s']/logs", task_id);
        short_path = g_strjoinv ("/", &entries[6]);
    } else {
        // We shouldn't have to specify recipe and task id since we
        // are searching from recipe node.
        logs_xpath = g_strdup_printf("task[@id='%s']/results/result[@id='%s']/logs",
                                     task_id, entries[6]);
        short_path = g_strjoinv ("/", &entries[8]);
    }

    gboolean content_range = soup_message_headers_get_content_range(
            headers, &start, &end, &total_length);

    gchar *basedir = g_path_get_dirname (filename);
    g_mkdir_with_parents (basedir, 0755 /* drwxr-xr-x */);
    g_free (basedir);

    if (content_range) {
        if (body->length != (end - start + 1)) {
//              soup_message_set_status_full(remote_msg,
//                    SOUP_STATUS_BAD_REQUEST,
//                    "Content length does not match range length");
            goto logs_cleanup;
        }
        if (total_length > 0 && total_length < end ) {
//            soup_message_set_status_full(remote_msg,
//                    SOUP_STATUS_BAD_REQUEST,
//                    "Total length is smaller than range end");
            goto logs_cleanup;
        }
        if (total_length > 0 && access( filename, F_OK ) != -1 ) {
            int result = truncate ((const char *)filename, total_length);
            g_warn_if_fail(result == 0);
        }
        if (start == 0) {
            // Record log in xml
            xmlXPathObjectPtr logs_node_ptrs = get_node_set(
                    app_data->xml_doc, recipe_data->recipe_node_ptr,
                    (xmlChar*)logs_xpath);
            if (logs_node_ptrs) {
                record_log(logs_node_ptrs->nodesetval->nodeTab[0],
                           log_path, short_path);
            }
            xmlXPathFreeObject (logs_node_ptrs);
        }
        update_chunk (filename, body->data, body->length, start);
    } else {
        if (access( filename, F_OK ) != -1 ) {
            int result = truncate ((const char *)filename, body->length);
            g_warn_if_fail(result == 0);
        }
        // Record log in xml
        xmlXPathObjectPtr logs_node_ptrs = get_node_set(app_data->xml_doc,
                recipe_data->recipe_node_ptr, (xmlChar *)logs_xpath);
        if (logs_node_ptrs) {
            record_log (logs_node_ptrs->nodesetval->nodeTab[0], log_path,
                        short_path);
        }
        xmlXPathFreeObject (logs_node_ptrs);
        update_chunk (filename, body->data, body->length, (goffset) 0);
    }
    const gchar *log_level_char = soup_message_headers_get_one(headers, "log-level");
    if (log_level_char) {
        gint log_level = g_ascii_strtoll (log_level_char, NULL, 0);
        if (app_data->verbose >= log_level) {
            ssize_t written = write (STDOUT_FILENO, body->data, body->length);
            g_warn_if_fail(written == body->length);
        }
    }
logs_cleanup:
    g_free (short_path);
    g_free (logs_xpath);
    g_free (log_path);
    g_free (filename);

    g_hash_table_destroy(table);
cleanup:
    g_free (task_id);
    g_free (recipe_id);
    g_strfreev (entries);
}

GSList *
register_path (GSList *callbacks, char *pattern, gpointer callback)
{
    RegexData *regex_data = g_slice_new0(RegexData);
    int reti;
    reti = regcomp (&regex_data->regex, pattern, REG_EXTENDED);
    if (reti) {
        g_slice_free (RegexData, regex_data);
        g_printerr ("Could not compile regex\n");
    } else {
        regex_data->callback = callback;
        callbacks = g_slist_append (callbacks, regex_data);
    }
    return callbacks;
}

gpointer
process_path (GSList *regexes, const char *path)
{
    int reti;
    while (regexes) {
        RegexData *regex_data = (RegexData *) regexes->data;
        reti = regexec (&regex_data->regex, path, 0, NULL, 0);
        if (!reti) {
            return regex_data->callback;
        }
        regexes = g_slist_next(regexes);
    }
    return NULL;
}

void
message_cb (const char *method,
            const char *path,
            GCancellable *cancellable,
            GError *error,
            SoupMessageHeaders *headers,
            SoupBuffer *body,
            gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*) user_data;
    AppData *app_data = recipe_data->app_data;
    RegexCallback callback;

    // path must be defined in order to dispatch
    if (!path)
        return;
    callback = process_path (app_data->regexes, path);
    if (callback)
        callback (method,
                  path,
                  cancellable,
                  error,
                  headers,
                  body,
                  user_data);
    else
        g_printerr ("no registered callback matches %s\n", path);
}

xmlDocPtr
get_doc (char *docname)
{
    xmlDocPtr doc;
    doc = xmlReadFile (docname, NULL, XML_PARSE_NOBLANKS);

    if (doc == NULL ) {
        fprintf(stderr,"Document not parsed successfully. \n");
        return NULL;
    }

    return doc;
}

static void
copy_task_nodes(xmlNodeSetPtr nodeset, xmlDocPtr orig_xml_doc,
                xmlDocPtr dst_xml_doc, xmlNodePtr dst_node_ptr)
{
    gchar *new_id;
    for (gint i=0; i < nodeset->nodeNr; i++) {
        xmlNodePtr task_node_ptr = xmlNewChild (dst_node_ptr,
                                                NULL,
                                                (xmlChar *) "task",
                                                NULL);
        // Add a logs node
        xmlNewChild (task_node_ptr,
                     NULL,
                     (xmlChar *) "logs",
                     NULL);

        // Copy <fetch> node if present.
        xmlNodePtr fetch_node_ptr = first_child_with_name (nodeset->nodeTab[i],
                                                           "fetch", FALSE);
        if (fetch_node_ptr) {
            xmlNodePtr copy_fetch_node_ptr = xmlDocCopyNode(fetch_node_ptr,
                                                            dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_fetch_node_ptr);
        }
        // Copy <rpm> node if present.
        xmlNodePtr rpm_node_ptr = first_child_with_name (nodeset->nodeTab[i],
                                                         "rpm", FALSE);
        if (rpm_node_ptr) {
            xmlNodePtr copy_rpm_node_ptr = xmlDocCopyNode(rpm_node_ptr,
                                                          dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_rpm_node_ptr);
        }
        // Copy <params> node if present.
        xmlNodePtr params_node_ptr = first_child_with_name(nodeset->nodeTab[i],
                                                          "params", FALSE);
        xmlNodePtr copy_params_node_ptr = NULL;
        if (params_node_ptr) {
            copy_params_node_ptr = xmlDocCopyNode(params_node_ptr,
                                                  dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_params_node_ptr);
        }

        xmlChar *name = xmlGetNoNsProp (nodeset->nodeTab[i], (xmlChar*)"name");
        xmlSetProp (task_node_ptr, (xmlChar *) "name", name);
        xmlChar *role = xmlGetNoNsProp(nodeset->nodeTab[i], (xmlChar*)"role");
        if (role != NULL) {
            xmlSetProp (task_node_ptr, (xmlChar*)"role", (xmlChar*)role);
            xmlFree(role);
        }
        new_id = g_strdup_printf ("%d", i + 1);
        xmlSetProp (task_node_ptr, (xmlChar *) "id", (xmlChar *) new_id);
        xmlSetProp (task_node_ptr, (xmlChar *) "status", (xmlChar *) "New");
        xmlSetProp (task_node_ptr, (xmlChar *) "result", (xmlChar *) "None");
        g_free(new_id);
        g_free(name);
    }
}

static void
parse_task_nodes (xmlNodeSetPtr nodeset, GHashTable *tasks)
{
    for (gint i=0; i < nodeset->nodeNr; i++) {
        xmlChar *id = xmlGetNoNsProp (nodeset->nodeTab[i], (xmlChar *)"id");
        g_hash_table_insert (tasks, id, nodeset->nodeTab[i]);
    }
}

// remove_ext: removes the "extension" from a file spec.
//   mystr is the string to process.
//   dot is the extension separator.
//   sep is the path separator (0 means to ignore).
// Returns an allocated string identical to the original but
//   with the extension removed. It must be freed when you're
//   finished with it.
// If you pass in NULL or the new string can't be allocated,
//   it returns NULL.
char *
remove_ext (char* mystr, char dot, char sep)
{
    char *retstr, *lastdot, *lastsep;

    // Error checks and allocate string.

    if (mystr == NULL)
        return NULL;
    if ((retstr = malloc (strlen (mystr) + 1)) == NULL)
        return NULL;

    // Make a copy and find the relevant characters.

    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

    // If it has an extension separator.

    if (lastdot != NULL) {
        // and it's before the extenstion separator.

        if (lastsep != NULL) {
            if (lastsep < lastdot) {
                // then remove it.

                *lastdot = '\0';
            }
        } else {
            // Has extension separator with no path separator.

            *lastdot = '\0';
        }
    }

    // Return the modified string.

    return retstr;
}

static gchar *
find_next_dir (gchar *basename)
{
    guint recipe_id = 1;
    gchar *file = g_strdup_printf ("./%s.%02d", basename, recipe_id);
    while (g_file_test (file, G_FILE_TEST_EXISTS)) {
        recipe_id += 1;
        g_free (file);
        file = g_strdup_printf ("./%s.%02d", basename, recipe_id);
    }
    gint ret = g_mkdir_with_parents (file, 0755 /* drwxr-xr-x */);
    if (ret != 0) {
        g_warning("Failed to make directory %s: %s", file, g_strerror(errno));
        exit (1);
    }
    return file;
}

static void
recipe_finish(RecipeData *recipe_data)
{
    AppData *app_data = recipe_data->app_data;
    GHashTableIter iter;
    xmlNodePtr task_node;

    g_hash_table_iter_init(&iter, recipe_data->tasks);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&task_node)) {
        xmlChar *status = xmlGetNoNsProp(task_node, (xmlChar*)"status");
        if (g_strcmp0((gchar *) status, "New") == 0 ||
            g_strcmp0((gchar *) status, "Running") == 0) {
            xmlSetProp(recipe_data->recipe_node_ptr, (xmlChar*)"status",
                       (xmlChar*)"Aborted");
            xmlSetProp(task_node, (xmlChar*)"status", (xmlChar*)"Aborted");
        }
        xmlFree(status);
        xmlChar *result = xmlGetNoNsProp (task_node, (xmlChar *)"result");
        if (g_strcmp0 ((gchar *) result, "PASS") != 0 && !app_data->error) {
            g_set_error (&app_data->error, RESTRAINT_ERROR,
                         RESTRAINT_TASK_RUNNER_RESULT_ERROR,
                         "One or more tasks failed");
        }
        xmlFree(result);
    }

    if (tasks_finished(app_data->xml_doc, NULL, (xmlChar *) "//task"))
        g_idle_add_full (G_PRIORITY_LOW,
                         quit_loop_handler,
                         app_data->loop,
                         NULL);
}

static gboolean
run_recipe_handler (gpointer user_data)
{
    RecipeData *recipe_data = (RecipeData*)user_data;
    AppData *app_data = recipe_data->app_data;
    gchar *full_url = NULL;
    // Tell restraintd to run our recipe
    SoupRequest *request;
    request = (SoupRequest *)soup_session_request_http_uri(session,
                                                           "POST",
                                                           recipe_data->remote_uri,
                                                           NULL);
    recipe_data->remote_msg = soup_request_http_get_message(
            SOUP_REQUEST_HTTP(request));

    // return the xml doc
    xmlBufferPtr buffer = xmlBufferCreate();
    gint size = xmlNodeDump(buffer, app_data->xml_doc, recipe_data->recipe_node_ptr, 0, 1);

    full_url = soup_uri_to_string (recipe_data->remote_uri, FALSE);
    g_print ("Connecting to %s for recipe id:%d\n", full_url, recipe_data->recipe_id);
    g_free (full_url);
    soup_message_set_request (recipe_data->remote_msg,
                              "text/xml",
                              SOUP_MEMORY_COPY, (const char *) buffer->content, size);
    xmlBufferFree (buffer);

    // turn off message body accumulating
    soup_message_body_set_accumulate (recipe_data->remote_msg->response_body, FALSE);
    multipart_request_send_async (request, recipe_data->cancellable, message_cb, remote_hup, recipe_data);

    return FALSE;
}

static xmlDocPtr
new_job ()
{
    xmlDocPtr xml_doc_ptr = xmlNewDoc ((xmlChar *) "1.0");
    xml_doc_ptr->children = xmlNewDocNode (xml_doc_ptr,
                                           NULL,
                                           (xmlChar *) "job",
                                           NULL);
    return xml_doc_ptr;
}

static xmlNodePtr
new_recipe (xmlDocPtr xml_doc_ptr, guint recipe_id,
            xmlNodePtr recipe_set_node_ptr, const xmlChar *wboard,
            const xmlChar *role)
{
    xmlNodePtr recipe_node_ptr = xmlNewTextChild (recipe_set_node_ptr,
                                       NULL,
                                       (xmlChar *) "recipe",
                                       NULL);
    gchar *new_id = g_strdup_printf ("%u", recipe_id);
    xmlSetProp (recipe_node_ptr, (xmlChar *)"id", (xmlChar *) new_id);
    xmlSetProp (recipe_node_ptr, (xmlChar *)"status", (xmlChar *) "New");
    xmlSetProp (recipe_node_ptr, (xmlChar *)"result", (xmlChar *) "None");
    if (wboard != NULL) {
        xmlSetProp(recipe_node_ptr, (xmlChar *)"whiteboard", wboard);
    }
    if (role != NULL) {
        xmlSetProp(recipe_node_ptr, (xmlChar *)"role", role);
    }
    g_free(new_id);
    return recipe_node_ptr;
}

static void add_r_params(gchar *role, GSList *hostlist,
                         xmlNodePtr r_params)
{
    gchar *hoststr = NULL;
    gchar **hostarr = g_new0(gchar *, g_slist_length(hostlist) + 1);
    gchar **p = hostarr;
    xmlNodePtr param = NULL;

    for (GSList *host = hostlist; host != NULL; host = g_slist_next(host)) {
        *p++ = host->data;
    }
    hoststr = g_strjoinv(" ", hostarr);

    xmlNode *child = r_params->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "param") == 0) {
            xmlChar *name = xmlGetNoNsProp(child, (xmlChar*)"name");
            if (g_strcmp0((gchar*)name, role) == 0) {
                param = child;
            }

            xmlFree(name);
            if (param != NULL) {
                break;
            }
        }
        child = child->next;
    }

    if (param == NULL) {
        param = xmlNewChild(r_params, NULL, (xmlChar*)"param",
                                       NULL);
    }
    xmlSetProp(param, (xmlChar*)"name", (xmlChar*)role);
    xmlSetProp(param, (xmlChar*)"value", (xmlChar*)hoststr);

    g_free(hoststr);
    g_free(hostarr);
}

static RecipeData *new_recipe_data(AppData *app_data, gchar *recipe_id)
{
    RecipeData *recipe_data = g_slice_new0(RecipeData);
    recipe_data->body = g_string_new(NULL);
    recipe_data->app_data = app_data;
    recipe_data->cancellable = g_cancellable_new();
    // Prime the watchdog handler, give us 5 minutes to get things
    // going.
    recipe_data->timeout_handler_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                                  300,
                                                                  watchdog_timeout_cb,
                                                                  recipe_data,
                                                                  NULL);
    g_hash_table_insert(app_data->recipes, g_strdup((gchar*)recipe_id),
                        recipe_data);
    return recipe_data;
}

static guint get_node_role(xmlNodePtr node, GHashTable *roletable,
                         RecipeData *recipe_data)
{
    xmlChar *role = xmlGetNoNsProp(node, (xmlChar*)"role");

    if (role != NULL) {
        gchar *host = (gchar*)soup_uri_get_host(recipe_data->remote_uri);
        GSList *hostlist = g_hash_table_lookup(roletable, role);
        if (hostlist == NULL) {
            hostlist = g_slist_prepend(hostlist, host);
            g_hash_table_insert(roletable, g_strdup((gchar*)role),
                                hostlist);
        } else {
            if (g_slist_find_custom(hostlist, host,
                        (GCompareFunc)g_strcmp0) == NULL) {
                GSList *tmp_list = g_slist_copy(hostlist);
                tmp_list = g_slist_prepend(tmp_list, host);
                g_hash_table_replace(roletable,
                                     g_strdup((gchar*)role),
                                     tmp_list);
            }
        }
        xmlFree(role);
        return 1;
    }
    return 0;
}

static GSList *get_recipe_members(GHashTable *roletable)
{
    GSList *allhosts = NULL;
    GList *rolehosts = g_hash_table_get_values(roletable);
    for (GList *rolehost = rolehosts; rolehost != NULL;
            rolehost = g_list_next(rolehost)) {
        for (GSList *host = (GSList*)rolehost->data; host != NULL;
                host = g_slist_next(host)) {
            if (g_slist_find_custom(allhosts, host->data,
                        (GCompareFunc)g_strcmp0) == NULL) {
                allhosts = g_slist_prepend(allhosts, host->data);
            }
        }
    }
    g_list_free(rolehosts);
    return allhosts;
}

static gchar *copy_job_as_template(gchar *job, AppData *app_data)
{
    guint recipe_id = 1;
    gchar *run_dir = NULL;

    // get xmldoc
    xmlDocPtr template_xml_doc_ptr = get_doc(job);
    if (!template_xml_doc_ptr) {
        g_printerr ("Unable to parse %s\n", job);
        xmlFreeDoc(template_xml_doc_ptr);
        return NULL;
    }
    xmlXPathObjectPtr recipe_nodes = get_node_set(template_xml_doc_ptr, NULL,
            (xmlChar*)"/job/recipeSet/recipe");

    xmlDocPtr new_xml_doc_ptr = new_job ();

    // Find next result dir job.0, job.1, etc..
    gchar *basename = g_path_get_basename (job);
    gchar *base = remove_ext (basename, '.', 0);
    run_dir = find_next_dir(base);
    g_print ("Using %s for job run\n", run_dir);
    g_free(basename);
    g_free(base);

    xmlNodePtr recipe_set_node_ptr = xmlNewTextChild(new_xml_doc_ptr->children,
                                                     NULL,
                                                     (xmlChar *) "recipeSet",
                                                     NULL);

    GHashTable *roletable = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              g_free,
                                              (GDestroyNotify)g_slist_free);

    for (guint i = 0; i < recipe_nodes->nodesetval->nodeNr; i++) {
        xmlNodePtr node = recipe_nodes->nodesetval->nodeTab[i];
        xmlChar *wboard = xmlGetNoNsProp(node, (xmlChar*)"whiteboard");
        xmlChar *id = xmlGetNoNsProp(node, (xmlChar*)"id");
        xmlChar *role = xmlGetNoNsProp(node, (xmlChar*)"role");

        if (id != NULL) {
            recipe_id = (guint)g_ascii_strtoull((gchar*)id, NULL, 0);
        } else {
            id = (xmlChar*)g_strdup_printf ("%u", recipe_id);
        }

        RecipeData *recipe_data = g_hash_table_lookup(app_data->recipes,
                                                      id);
        if (recipe_data == NULL) {
            g_printerr ("Unable to find matching host for recipe id:%s did you pass --host on the cmd line?\n", id);
            xmlFreeDoc(template_xml_doc_ptr);
            g_hash_table_destroy(roletable);
            return NULL;
        }

        guint recipe_role = get_node_role(node, roletable, recipe_data);
        xmlNodePtr new_recipe_ptr = new_recipe(new_xml_doc_ptr, recipe_id++,
                                       recipe_set_node_ptr, wboard, role);

        // find task nodes
        xmlXPathObjectPtr task_nodes = get_node_set(template_xml_doc_ptr,
                node, (xmlChar*)"task");
        if (task_nodes) {
            copy_task_nodes(task_nodes->nodesetval, template_xml_doc_ptr,
                            new_xml_doc_ptr, new_recipe_ptr);

            for (guint j = 0; j < task_nodes->nodesetval->nodeNr; j++) {
                xmlNodePtr tnode = task_nodes->nodesetval->nodeTab[j];
                if (recipe_role == 0) {
                    get_node_role(tnode, roletable, recipe_data);
                }
            }
        }

        xmlXPathFreeObject(task_nodes);
        xmlFree(role);
        xmlFree(wboard);
        xmlFree(id);
    }

    xmlNodePtr r_params = xmlNewDocNode(new_xml_doc_ptr, NULL,
                                        (xmlChar*)"params", NULL);
    g_hash_table_foreach(roletable, (GHFunc)add_r_params, r_params);
    GSList *recipemembers = get_recipe_members(roletable);
    add_r_params("RECIPE_MEMBERS", recipemembers, r_params);

    xmlXPathFreeObject(recipe_nodes);
    recipe_nodes = get_node_set(new_xml_doc_ptr, NULL,
                                (xmlChar*)"/job/recipeSet/recipe");
    for (guint i = 0; i < recipe_nodes->nodesetval->nodeNr; i++) {
        xmlNodePtr rnode = recipe_nodes->nodesetval->nodeTab[i];

        xmlXPathObjectPtr task_nodes = get_node_set(new_xml_doc_ptr,
                rnode, (xmlChar*)"task");
        if (task_nodes) {
            for (guint j = 0; j < task_nodes->nodesetval->nodeNr; j++) {
                xmlNodePtr tnode = task_nodes->nodesetval->nodeTab[j];
                xmlNodePtr t_params = first_child_with_name(tnode, "params",
                                                            FALSE);
                if (t_params == NULL) {
                    xmlAddChild(tnode, xmlCopyNode(r_params, 1));
                } else {
                    g_hash_table_foreach(roletable, (GHFunc)add_r_params,
                                         t_params);
                    add_r_params("RECIPE_MEMBERS", recipemembers, t_params);
                }
            }
        }
        xmlXPathFreeObject(task_nodes);
    }

    g_slist_free(recipemembers);
    g_hash_table_destroy(roletable);
    xmlFreeNode(r_params);

    // Write out our new job.
    gchar *filename = g_build_filename (run_dir, "job.xml", NULL);
    put_doc (new_xml_doc_ptr, filename);
    g_free (filename);
    xmlXPathFreeObject(recipe_nodes);
    xmlFreeDoc(template_xml_doc_ptr);
    xmlFreeDoc(new_xml_doc_ptr);

    return run_dir;
}

static gboolean remove_extra_recipes(gchar *id, RecipeData *recipe_data,
                                     GSList *idlist)
{
    if (g_slist_find_custom(idlist, id, (GCompareFunc)g_strcmp0) == NULL) {
        return TRUE;
    }
    return FALSE;
}

static void
parse_new_job (AppData *app_data)
{
    gchar *filename = g_build_filename (app_data->run_dir, "job.xml", NULL);

    // get xmldoc
    app_data->xml_doc = get_doc(filename);
    if (!app_data->xml_doc) {
        g_printerr ("Unable to parse %s\n", filename);
        g_free (filename);
        xmlFreeDoc(app_data->xml_doc);
        return;
    }

    // find recipe nodes
    xmlXPathObjectPtr recipe_node_ptrs = get_node_set(app_data->xml_doc, NULL,
            (xmlChar*)"/job/recipeSet/recipe");
    if (!recipe_node_ptrs) {
        g_printerr ("No <recipe> element in %s\n", filename);
        g_free (filename);
        xmlXPathFreeObject (recipe_node_ptrs);
        xmlFreeDoc(app_data->xml_doc);
        return;
    }

    GSList *idlist = NULL;
    for (gint i = 0; i < recipe_node_ptrs->nodesetval->nodeNr; i++) {
        xmlNodePtr node = recipe_node_ptrs->nodesetval->nodeTab[i];
        xmlChar *recipe_id = xmlGetNoNsProp(node, (xmlChar *)"id");

        idlist = g_slist_prepend(idlist, g_strdup((gchar*)recipe_id));
        RecipeData *recipe_data = g_hash_table_lookup(app_data->recipes,
                                                      recipe_id);
        if (recipe_data == NULL) {
            g_printerr ("Unable to find matching recipe id:%s in job.\n", recipe_id);
            g_free (filename);
            g_free (recipe_id);
            g_slist_free_full(idlist, g_free);
            xmlXPathFreeObject (recipe_node_ptrs);
            xmlFreeDoc(app_data->xml_doc);
            return;
        }
        recipe_data->recipe_node_ptr = node;
        recipe_data->recipe_id = (gint)g_ascii_strtoll((gchar *)recipe_id,
                                                        NULL, 0);
        g_free (recipe_id);

        if (app_data->addr_get_uri == NULL) {
            app_data->addr_get_uri = recipe_data->remote_uri;
        }

        // find task nodes
        xmlXPathObjectPtr task_nodes = get_node_set(app_data->xml_doc, node,
                                                    (xmlChar*)"task");
        if (!task_nodes) {
            g_printerr ("No <task> element(s) in %s\n", filename);
            g_free (filename);
            g_slist_free_full(idlist, g_free);
            xmlXPathFreeObject (recipe_node_ptrs);
            xmlXPathFreeObject (task_nodes);
            xmlFreeDoc(app_data->xml_doc);
            return;
        }
        recipe_data->tasks = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   xmlFree, NULL);

        // record each task in a hash table
        parse_task_nodes(task_nodes->nodesetval, recipe_data->tasks);
        xmlXPathFreeObject (task_nodes);
    }
    g_hash_table_foreach_remove(app_data->recipes,
                                (GHRFunc)&remove_extra_recipes,
                                idlist);
    g_slist_free_full(idlist, g_free);
    xmlXPathFreeObject (recipe_node_ptrs);
    g_free (filename);
}

static gboolean
callback_parse_verbose (const gchar *option_name, const gchar *value,
        gpointer user_data, GError **error)
{
    AppData *app_data = (AppData *) user_data;
    app_data->verbose++;
    return TRUE;
}

void
pretty_results (gchar* run_dir)
{
    gchar* cmdline = NULL;
    gchar *bootstrap_src = "/usr/share/restraint/client/bootstrap/bootstrap.min.css";
    gchar *bootstrap = NULL;
    gchar* job2html = "/usr/share/restraint/client/job2html.xml";
    gchar* jobxml = NULL;
    gchar* std_out = NULL;
    gchar *contents = NULL;
    gsize length;
    gchar* std_err = NULL;
    gint exitstat;
    GError *gerror = NULL;
    gchar* results_filename = NULL;
    gint results_fd;
    gssize results_len;
    gboolean success = FALSE;

    success = g_file_get_contents (bootstrap_src,
                                   &contents,
                                   &length,
                                   &gerror);
    if (!success) {
        g_printerr ("Error opening %s for reading: %s\n",
                    bootstrap_src,
                    gerror->message);
        goto cleanup;
    }

    bootstrap = g_build_filename (run_dir, "bootstrap.min.css", NULL);
    success = g_file_set_contents (bootstrap,
                                   contents,
                                   length,
                                   &gerror);
    if (!success) {
        g_printerr ("Error opening %s for writing: %s\n",
                    bootstrap,
                    gerror->message);
        goto cleanup;
    }

    jobxml = g_build_filename (run_dir, "job.xml", NULL);
    cmdline = g_strdup_printf("xsltproc %s %s", job2html, jobxml);

    if (!g_spawn_command_line_sync(cmdline, &std_out, &std_err, &exitstat,
                                   &gerror)) {
        g_printerr("cannot spawn '%s': %s\n", cmdline, gerror->message);
        goto cleanup;
    }
    if (exitstat != 0) {
        g_printerr("xsltproc error: %s\n", std_err);
        goto cleanup;
    }

    results_filename = g_build_filename (run_dir, "index.html", NULL);
    results_fd = g_open(results_filename,
                        O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (results_fd == -1) {
        g_printerr("Error opening %s for writing\n", results_filename);
        goto cleanup;
    }
    length = strlen(std_out);
    results_len = write(results_fd, std_out, length);
    if(!g_close(results_fd, &gerror)) {
        g_printerr("cannot close %s: %s\n", results_filename, gerror->message);
        goto cleanup;
    }
    if (results_len != length) {
        g_printerr("Error writing to %s\n", results_filename);
    }

cleanup:
    if (gerror != NULL)
        g_error_free(gerror);
    if (results_filename != NULL)
        g_free (results_filename);
    if (jobxml != NULL)
        g_free (jobxml);
    if (bootstrap != NULL)
        g_free (bootstrap);
    if (contents != NULL)
        g_free (contents);
    if (cmdline != NULL)
        g_free (cmdline);
    if (std_out != NULL)
        g_free (std_out);
    if (std_err != NULL)
        g_free (std_err);
}

static gboolean add_recipe_host(const gchar *option_name, const gchar *value,
                                gpointer data, GError **error) {
    AppData *app_data = (AppData*)data;
    gchar **args;
    gchar *remote;
    gboolean result = TRUE;
    SoupURI *remote_uri;

    args = g_strsplit(value, "=", 2);
    if (g_strv_length(args) != 2) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            "Option format not recognized: '%s'. Try --help.", value);
        g_strfreev(args);
        return FALSE;
    }

    if (g_strrstr(args[1], ":") == NULL) {
        remote = g_strdup_printf("http://%s:%d/run", args[1], DEFAULT_PORT);
    } else {
        remote = g_strdup_printf("http://%s/run", args[1]);
    }

    remote_uri = soup_uri_new(remote);
    if (remote_uri == NULL) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
            "Wrong URI format: %s.", remote);
        result = FALSE;
        goto cleanup;
    }

    RecipeData *recipe_data = new_recipe_data(app_data, args[0]);
    recipe_data->remote_uri = remote_uri;

cleanup:
    g_free(remote);
    g_strfreev(args);
    return result;
}

static void recipe_init(gchar *id, RecipeData *recipe_data,
                        gpointer *user_data)
{
    // Request to run the recipe.
    g_idle_add_full(G_PRIORITY_LOW,
                    run_recipe_handler,
                    recipe_data,
                    NULL);
}

int main(int argc, char *argv[]) {
    gchar *job = NULL;
    gboolean ipv6 = FALSE;

    AppData *app_data = g_slice_new0 (AppData);

    init_result_hash (app_data);
    app_data->recipes = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, (GDestroyNotify)&restraint_free_recipe_data);

    GOptionEntry entries[] = {
        {"ipv6", '6', 0, G_OPTION_ARG_NONE, &ipv6,
            "Use IPV6 for communication", NULL },
        {"port", 'p', 0, G_OPTION_ARG_INT, &app_data->port,
            "Specify the port to listen on", "PORT" },
        { "job", 'j', 0, G_OPTION_ARG_STRING, &job,
            "Run job from file", "FILE" },
        { "run", 'r', 0, G_OPTION_ARG_STRING, &app_data->run_dir,
            "Continue interrupted job from DIR", "DIR" },
        { "host", 't', 0, G_OPTION_ARG_CALLBACK, &add_recipe_host,
            "Set host for a recipe with specific id.",
            "<recipe_id>=<host>[:<port>]" },
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
            callback_parse_verbose, "Increase verbosity, up to three times.",
            NULL },
        { NULL }
    };
    GOptionGroup *option_group = g_option_group_new("main",
            "Application Options", "Various application related options",
            app_data, NULL);
    GOptionContext *context = g_option_context_new(NULL);

    g_option_context_set_summary (context,
            "Test harness for Beaker. Runs tasks according to a job \n"
            "and collects their results.");
    g_option_group_add_entries(option_group, entries);
    g_option_context_set_main_group (context, option_group);

    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv,
            &app_data->error);
    g_option_context_free(context);

    if (ipv6) {
        app_data->address_family = SOUP_ADDRESS_FAMILY_IPV6;
    } else {
        app_data->address_family = SOUP_ADDRESS_FAMILY_IPV4;
    }

    SoupAddress *address = soup_address_new_any(app_data->address_family,
                                                SOUP_ADDRESS_ANY_PORT);

    session = soup_session_new_with_options("local-address", address,
                                            "timeout", 120,
                                            "idle-timeout", 120,
                                            "max-conns", 20,
                                            "max-conns-per-host", 1,
                                            NULL);

    if (app_data->run_dir && app_data->port) {
        g_printerr ("You can't specify both run_dir and port\n");
        g_printerr ("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    if (job) {
        // if template job is passed in use it to generate our job
        app_data->run_dir = copy_job_as_template(job, app_data);
    }
    if (!parse_succeeded || !app_data->run_dir) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    // Read in run_dir/job.xml
    parse_new_job (app_data);

    // If all tasks are finished then quit.
    if (tasks_finished(app_data->xml_doc, NULL, (xmlChar *) "//task")) {
        g_printerr ("All tasks are finished\n");
        goto cleanup;
    }

    g_hash_table_foreach(app_data->recipes, (GHFunc)&recipe_init, NULL);

    app_data->regexes = register_path (app_data->regexes,
                                       "/recipes/[[:digit:]]+/tasks/[[:digit:]]+/status$",
                                       tasks_status_cb);
    app_data->regexes = register_path (app_data->regexes,
                                       "/recipes/[[:digit:]]+/tasks/[[:digit:]]+/results/$",
                                       tasks_results_cb);
    app_data->regexes = register_path (app_data->regexes,
                                       "/recipes/[[:digit:]]+/watchdog$",
                                       watchdog_cb);
    app_data->regexes = register_path (app_data->regexes,
                                       "/recipes/[[:digit:]]+/tasks/[[:digit:]]+/logs/",
                                       tasks_logs_cb);
    app_data->regexes = register_path (app_data->regexes,
                                       "/recipes/[[:digit:]]+/tasks/[[:digit:]]+/results/[[:digit:]]+/logs/",
                                       tasks_logs_cb);
    // Create and enter the main loop
    app_data->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(app_data->loop);

    gchar *filename = g_build_filename (app_data->run_dir, "job.xml", NULL);
    put_doc (app_data->xml_doc, filename);
    g_free (filename);

    // We're done.
    xmlFreeDoc(app_data->xml_doc);
    xmlCleanupParser();

    // convert job.xml to index.html
    pretty_results(app_data->run_dir);

cleanup:
    soup_session_abort(session);
    g_object_unref(session);
    g_object_unref(address);
    if (job != NULL) {
        g_free(job);
    }
    if (app_data->error) {
        int retcode = app_data->error->code;
        g_printerr("%s [%s, %d]\n", app_data->error->message,
                g_quark_to_string(app_data->error->domain),
                app_data->error->code);
        restraint_free_app_data(app_data);
        return retcode;
    } else {
        restraint_free_app_data(app_data);
        return EXIT_SUCCESS;
    }
}
