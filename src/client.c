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

#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "client.h"

static SoupSession *session;
gchar buf[1024];

// Select first recipe #FIXME - would be nice to support guest recipes too!
xmlChar *task_xpath = (xmlChar *) "/job/recipeSet/recipe[1]/task";
xmlChar *recipe_xpath = (xmlChar *) "/job/recipeSet/recipe[1]";

#define RESTRAINT_CLIENT_ERROR restraint_client_error()
GQuark
restraint_client_error(void) {
    return g_quark_from_static_string("restraint-client-error");
}
#define RESTRAINT_CLIENT_STREAM_ERROR restraint_client_stream_error()
GQuark
restraint_client_stream_error(void) {
    return g_quark_from_static_string("restraint-client-stream-error");
}

xmlXPathObjectPtr get_node_set (xmlDocPtr doc, xmlChar *xpath);

static void
recipe_callback (SoupServer *server, SoupMessage *remote_msg,
                 const char *path, GHashTable *query,
                 SoupClientContext *context, gpointer data)
{
    AppData *app_data = (AppData *) data;
    xmlChar *buf;
    gint size;

    // return the xml doc
    xmlDocDumpMemory (app_data->xml_doc, &buf, &size);
    soup_message_body_append (remote_msg->response_body,
                              SOUP_MEMORY_COPY,
                              buf, (gsize) size);
    xmlFree (buf);
    soup_message_set_status (remote_msg, SOUP_STATUS_OK);
}

static void
update_chunk (gchar *filename, const gchar *data, gsize size, goffset offset)
{
    gint fd = g_open(filename, O_WRONLY | O_CREAT, 0644);
    lseek (fd, offset, SEEK_SET);
    write (fd, data, size);
    g_close (fd, NULL);
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
tasks_finished (GHashTable *tasks)
{
    GHashTableIter iter;
    gpointer key, value;
    gboolean finished = TRUE;

    g_hash_table_iter_init (&iter, tasks);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        gchar *status = (gchar *)xmlGetNoNsProp (value, (xmlChar *)"status");
        finished &= (strcmp (status, "Completed") == 0 || strcmp (status, "Aborted") == 0);
        g_free (status);
    }
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

static gint
record_result (xmlNodePtr results_node_ptr,
               const gchar *result,
               const gchar *message,
               const gchar *path,
               const gchar *score,
               gint verbose)
{
    static int result_id = 0;
    result_id++;

    gchar *result_id_str = g_strdup_printf ("%d", result_id);

    // record result under task_node_ptr
    xmlNodePtr result_node_ptr = xmlNewTextChild (results_node_ptr,
                                                  NULL,
                                                  (xmlChar *) "result",
                                                  (xmlChar *) message);
    xmlSetProp (result_node_ptr, (xmlChar *)"id", (xmlChar *) result_id_str);
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
        g_print ("**   %4s [%-48s] %s", result_id_str,
                 offset_path, result);
        if (score != NULL) {
            g_print (" Score: %s", score);
        }
        g_print ("\n");
        if (message) {
            g_print ("**            %s\n", message);
        }
    }
    return result_id;
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
    xmlDocFormatDump (outxml, xml_doc, 1);
    fclose (outxml);
}

static xmlNodePtr
first_child_with_name (xmlNodePtr parent_ptr, const gchar *name, gboolean create)
{
    xmlNodePtr results_node_ptr = NULL;
    xmlNode *child = parent_ptr->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, name) == 0)
            return child;
        child = child->next;
    }
    // If requested create if not found
    if (create) {
        results_node_ptr = xmlNewTextChild (parent_ptr,
                                            NULL,
                                            (xmlChar *) name,
                                            NULL);
    }
    return results_node_ptr;
}

static void
task_callback (SoupServer *server, SoupMessage *remote_msg,
                 const char *path, GHashTable *query,
                 SoupClientContext *context, gpointer data)
{
    AppData *app_data = (AppData *) data;
    GHashTable *table;

    // Pull some values out of the path.
    gchar **entries = g_strsplit (path, "/", 0);
    gchar *task_id = g_strdup (entries[4]);
    gchar *recipe_id = g_strdup (entries[2]);

    // Lookup our task
    xmlNodePtr task_node_ptr = g_hash_table_lookup (app_data->tasks, task_id);
    if (!task_node_ptr) {
        soup_message_set_status_full (remote_msg, SOUP_STATUS_BAD_REQUEST, "Invalid Task!");
        goto cleanup;
    }

    table = soup_form_decode (remote_msg->request_body->data);
    // Record results
    if (g_str_has_suffix (path, "/results/")) {

        gchar *result = g_hash_table_lookup (table, "result");
        gchar *message = g_hash_table_lookup (table, "message");
        gchar *path = g_hash_table_lookup (table, "path");
        gchar *score = g_hash_table_lookup (table, "score");

        xmlNodePtr results_node_ptr = first_child_with_name (task_node_ptr, "results", TRUE);
        // Record the result and get our result id back.
        gint result_id = record_result (results_node_ptr, result, message, path, score, app_data->verbose);

        gchar *recipe_result = (gchar *)xmlGetNoNsProp (app_data->recipe_node_ptr, (xmlChar *) "result");
        gchar *task_result = (gchar *)xmlGetNoNsProp (task_node_ptr, (xmlChar *) "result");

        // Push higher priority results up the chain, result->task->recipe.
        if (result_to_id (result, app_data->result_states_to) > result_to_id (task_result, app_data->result_states_to))
            xmlSetProp (task_node_ptr, (xmlChar *)"result", (xmlChar *) result);
        if (result_to_id (result, app_data->result_states_to) > result_to_id (recipe_result, app_data->result_states_to))
            xmlSetProp (app_data->recipe_node_ptr, (xmlChar *)"result", (xmlChar *) result);

        // set result Location
        gchar *result_url = g_strdup_printf ("%srecipes/%s/tasks/%s/results/%d",
                                             app_data->address,
                                             recipe_id,
                                             task_id,
                                             result_id);
        soup_message_headers_append (remote_msg->response_headers, "Location", result_url);
        soup_message_set_status (remote_msg, SOUP_STATUS_CREATED);
    } else if (g_str_has_suffix (path, "/status")) {
        gchar *status = g_hash_table_lookup (table, "status");
        gchar *message = g_hash_table_lookup (table, "message");
        if (app_data->verbose < 2) {
            xmlChar *task_name = xmlGetNoNsProp (task_node_ptr, (xmlChar *)"name");
            xmlChar *task_result = xmlGetNoNsProp (task_node_ptr, (xmlChar *)"result");
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
            xmlNodePtr results_node_ptr = first_child_with_name (task_node_ptr, "results", TRUE);
            record_result (results_node_ptr, "Warn", message, "/", NULL, app_data->verbose);
        }

        xmlSetProp (task_node_ptr, (xmlChar *)"status", (xmlChar *) status);
        gchar *recipe_status = (gchar *)xmlGetNoNsProp (app_data->recipe_node_ptr, (xmlChar *) "status");

        // If recipe status is not already "Aborted" then record push task status to recipe.
        if (g_strcmp0 (recipe_status, "Aborted") != 0) 
            xmlSetProp (app_data->recipe_node_ptr, (xmlChar *)"status", (xmlChar *) status);

        // Write out the current version of the results xml.
        gchar *filename = g_build_filename (app_data->run_dir, "job.xml", NULL);
        put_doc (app_data->xml_doc, filename);

        // If all tasks are finished then quit.
        if (tasks_finished (app_data->tasks))
            g_idle_add_full (G_PRIORITY_LOW,
                             quit_loop_handler,
                             app_data->loop,
                             NULL);

        soup_message_set_status (remote_msg, SOUP_STATUS_NO_CONTENT);
    } else if (g_strrstr (path, "/logs/") != NULL) {
        goffset start;
        goffset end;
        goffset total_length;
        gchar *short_path = NULL;
        gchar *log_path = g_strjoinv ("/", &entries[4]);
        gchar *filename = g_strdup_printf ("%s/%s", app_data->run_dir, log_path);

        gchar *logs_xpath = NULL;
        if (g_strcmp0 (entries[5], "logs") == 0) {
            logs_xpath = g_strdup_printf("//task[contains(@id,'%s')]/logs", task_id);
            short_path = g_strjoinv ("/", &entries[6]);
        } else {
            logs_xpath = g_strdup_printf("//result[contains(@id,'%s')]/logs", entries[6]);
            short_path = g_strjoinv ("/", &entries[8]);
        }

        gboolean content_range = soup_message_headers_get_content_range (remote_msg->request_headers,
                                                                         &start, &end, &total_length);
        SoupBuffer *body = soup_message_body_flatten (remote_msg->request_body);

        gchar *basedir = g_path_get_dirname (filename);
        g_mkdir_with_parents (basedir, 0755 /* drwxr-xr-x */);
        g_free (basedir);

        if (content_range) {
            if (body->length != (end - start + 1)) {
                soup_message_set_status_full (remote_msg,
                                              SOUP_STATUS_BAD_REQUEST,
                                              "Content length does not match range length");
                return;
            }
            if (total_length > 0 && total_length < end ) {
                soup_message_set_status_full (remote_msg,
                                              SOUP_STATUS_BAD_REQUEST,
                                              "Total length is smaller than range end");
                return;
            }
            if (total_length > 0) {
                truncate ((const char *)filename, total_length);
            }
            if (start == 0) {
                // Record log in xml
                xmlXPathObjectPtr logs_node_ptrs = get_node_set (app_data->xml_doc,
                                                                 (xmlChar *)logs_xpath);
                if (logs_node_ptrs) {
                    record_log (logs_node_ptrs->nodesetval->nodeTab[0], log_path, short_path);
                }
                xmlXPathFreeObject (logs_node_ptrs);
            }
            update_chunk (filename, body->data, body->length, start);
        } else {
            truncate ((const char *)filename, body->length);
            // Record log in xml
            xmlXPathObjectPtr logs_node_ptrs = get_node_set (app_data->xml_doc,
                                                             (xmlChar *)logs_xpath);
            if (logs_node_ptrs) {
                record_log (logs_node_ptrs->nodesetval->nodeTab[0], log_path, short_path);
            }
            xmlXPathFreeObject (logs_node_ptrs);
            update_chunk (filename, body->data, body->length, (goffset) 0);
        }
        const gchar *log_level_char = soup_message_headers_get_one (remote_msg->request_headers, "log-level");
        if (log_level_char) {
            gint log_level = g_ascii_strtoll (log_level_char, NULL, 0);
            if (app_data->verbose >= log_level) {
                write (STDOUT_FILENO, body->data, body->length);
            }
        }
        g_free (short_path);
        g_free (log_path);
        g_free (filename);
        soup_buffer_free (body);
        soup_message_set_status (remote_msg, SOUP_STATUS_NO_CONTENT);
    }

cleanup:
    g_free (task_id);
    g_free (recipe_id);
    g_strfreev (entries);
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

xmlXPathObjectPtr
get_node_set (xmlDocPtr doc, xmlChar *xpath)
{
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        printf("No result\n");
        return NULL;
    }
    return result;
}

static void
copy_task_nodes (xmlNodeSetPtr nodeset, xmlDocPtr orig_xml_doc, xmlDocPtr dst_xml_doc, xmlNodePtr dst_node_ptr)
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
            xmlNodePtr copy_fetch_node_ptr = xmlDocCopyNode (fetch_node_ptr, dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_fetch_node_ptr);
        }
        // Copy <rpm> node if present.
        xmlNodePtr rpm_node_ptr = first_child_with_name (nodeset->nodeTab[i],
                                                         "rpm", FALSE);
        if (rpm_node_ptr) {
            xmlNodePtr copy_rpm_node_ptr = xmlDocCopyNode (rpm_node_ptr, dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_rpm_node_ptr);
        }
        // Copy <params> node if present.
        xmlNodePtr params_node_ptr = first_child_with_name (nodeset->nodeTab[i],
                                                           "params", FALSE);
        xmlNodePtr copy_params_node_ptr = NULL;
        if (params_node_ptr) {
            copy_params_node_ptr = xmlDocCopyNode (params_node_ptr, dst_xml_doc, 1);
            xmlAddChild (task_node_ptr, copy_params_node_ptr);
        }

        xmlChar *name = xmlGetNoNsProp (nodeset->nodeTab[i], (xmlChar *) "name");
        xmlSetProp (task_node_ptr, (xmlChar *) "name", name);
        new_id = g_strdup_printf ("%d", i + 1);
        xmlSetProp (task_node_ptr, (xmlChar *) "id", (xmlChar *) new_id);
        xmlSetProp (task_node_ptr, (xmlChar *) "status", (xmlChar *) "New");
        xmlSetProp (task_node_ptr, (xmlChar *) "result", (xmlChar *) "None");
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
find_next_dir (gchar *basename, gint *recipe_id)
{
    *recipe_id = 1;
    gchar *file = g_strdup_printf ("./%s.%02d", basename, *recipe_id);
    while (g_file_test (file, G_FILE_TEST_EXISTS)) {
        *recipe_id += 1;
        g_free (file);
        file = g_strdup_printf ("./%s.%02d", basename, *recipe_id);
    }
    gint ret = g_mkdir_with_parents (file, 0755 /* drwxr-xr-x */);
    if (ret != 0) {
        g_warning ("Failed to make directory %s: %s", file, g_strerror (errno));
        exit (1);
    }
    return file;
}

static void
recipe_read_closed (GObject *source, GAsyncResult *res, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GInputStream *stream = G_INPUT_STREAM (source);
    GError *error = NULL;

    g_print ("%s", app_data->body->str);
    if (g_strrstr (app_data->body->str, "* WARNING **:") != 0) {
        g_main_loop_quit (app_data->loop);
    }
    
    g_string_free (app_data->body, TRUE);
    g_input_stream_close_finish (stream, res, &error);
    g_object_unref (stream);
}

static void
recipe_read_data (GObject *source, GAsyncResult *res, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GInputStream *stream = G_INPUT_STREAM (source);
    GError *error = NULL;
    gsize nread;
    GString *body = app_data->body;

    nread = g_input_stream_read_finish (stream, res, &error);
    if (nread == -1) {
        g_printerr ("%s\n", error->message);
        g_clear_error (&error);
        g_input_stream_close (stream, NULL, NULL);
        g_object_unref (stream);
        g_main_loop_quit (app_data->loop);
        return;
    } else if (nread == 0) {
        g_input_stream_close_async (stream,
                                    G_PRIORITY_DEFAULT, NULL,
                                    recipe_read_closed, app_data);
        return;
    }

    g_string_append_len (body, buf, nread);
    g_input_stream_read_async (stream, buf, sizeof (buf),
                              G_PRIORITY_DEFAULT, NULL,
                              recipe_read_data, app_data);
}

static void
run_recipe_sent (GObject *source, GAsyncResult *res, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    SoupRequest *request = SOUP_REQUEST (source);
    GInputStream *stream;
    GError *error = NULL;

    SoupMessage *remote_msg = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
    if (!SOUP_STATUS_IS_SUCCESSFUL (remote_msg->status_code)) {
        g_printerr ("* WARNING **: %s\n", remote_msg->reason_phrase);
        g_main_loop_quit (app_data->loop);
        return;
    }

    stream = soup_request_send_finish (request, res, &error);
    if (!stream) {
        g_printerr ("* WARNING **: %s\n", error->message);
        g_clear_error (&error);
        g_main_loop_quit (app_data->loop);
        return;
    }
    g_input_stream_read_async (stream, buf, sizeof (buf),
                               G_PRIORITY_DEFAULT, NULL,
                               recipe_read_data, app_data);
}

static gboolean
run_recipe_handler (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GHashTable *data_table = NULL;
    gchar *form_data;
    // Tell restraintd to run our recipe
    SoupRequest *request;
    SoupURI *uri = soup_uri_new_with_base (app_data->remote_uri, "control");

    request = (SoupRequest *)soup_session_request_http_uri (session, "POST", uri, NULL);
    app_data->remote_msg = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));

    soup_uri_free (uri);

    data_table = g_hash_table_new (NULL, NULL);

    gchar *recipe_url = g_strdup_printf ("%srecipes/%d/", app_data->address, app_data->recipe_id);
    g_hash_table_insert (data_table, "recipe", recipe_url);
    form_data = soup_form_encode_hash (data_table);
    soup_message_set_request (app_data->remote_msg, "application/x-www-form-urlencoded",
                              SOUP_MEMORY_TAKE, form_data, strlen (form_data));

    soup_request_send_async (request, NULL, run_recipe_sent, app_data);
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
new_recipe (xmlDocPtr xml_doc_ptr, gint recipe_id)
{
    xmlNodePtr recipe_set_node_ptr = xmlNewTextChild (xml_doc_ptr->children,
                                                  NULL,
                                                  (xmlChar *) "recipeSet",
                                                  NULL);
    xmlNodePtr recipe_node_ptr = xmlNewTextChild (recipe_set_node_ptr,
                                       NULL,
                                       (xmlChar *) "recipe",
                                       NULL);
    gchar *new_id = g_strdup_printf ("%d", recipe_id);
    xmlSetProp (recipe_node_ptr, (xmlChar *)"id", (xmlChar *) new_id);
    xmlSetProp (recipe_node_ptr, (xmlChar *)"status", (xmlChar *) "New");
    xmlSetProp (recipe_node_ptr, (xmlChar *)"result", (xmlChar *) "None");
    return recipe_node_ptr;
}

static gchar *
copy_job_as_template (gchar *job)
{
    gint recipe_id;
    gchar *run_dir = NULL;

    // get xmldoc
    xmlDocPtr template_xml_doc_ptr = get_doc(job);
    if (!template_xml_doc_ptr) {
        g_printerr ("Unable to parse %s\n", job);
        xmlFreeDoc(template_xml_doc_ptr);
        return NULL;
    }

    // find task nodes
    xmlXPathObjectPtr node_ptrs = get_node_set (template_xml_doc_ptr, task_xpath);
    if (!node_ptrs) {
        g_printerr ("No <task> element(s) in %s\n", job);
        xmlXPathFreeObject (node_ptrs);
        xmlFreeDoc(template_xml_doc_ptr);
        return NULL;
    }

    // Find next result dir job.0, job.1, etc..
    gchar *basename = g_path_get_basename (job);
    gchar *base = remove_ext (basename, '.', 0);
    run_dir = find_next_dir (base, &recipe_id);
    g_print ("Using %s for job run\n", run_dir);

    // Create new job based on template job.
    xmlDocPtr new_xml_doc_ptr = new_job ();
    xmlNodePtr new_recipe_ptr = new_recipe (new_xml_doc_ptr, recipe_id);

    // Copy our task nodes from our template job.
    copy_task_nodes (node_ptrs->nodesetval, template_xml_doc_ptr, new_xml_doc_ptr, new_recipe_ptr);
    xmlXPathFreeObject (node_ptrs);

    // Write out our new job.
    gchar *filename = g_build_filename (run_dir, "job.xml", NULL);
    put_doc (new_xml_doc_ptr, filename);
    g_free (filename);
    xmlFreeDoc(template_xml_doc_ptr);
    xmlFreeDoc(new_xml_doc_ptr);

    return run_dir;
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

    // find recipe node
    xmlXPathObjectPtr recipe_node_ptrs = get_node_set (app_data->xml_doc, recipe_xpath);
    if (!recipe_node_ptrs) {
        g_printerr ("No <recipe> element in %s\n", filename);
        g_free (filename);
        xmlXPathFreeObject (recipe_node_ptrs);
        xmlFreeDoc(app_data->xml_doc);
        return;
    }
    app_data->recipe_node_ptr = recipe_node_ptrs->nodesetval->nodeTab[0];
    xmlChar *recipe_id = xmlGetNoNsProp(app_data->recipe_node_ptr, (xmlChar *)"id");
    app_data->recipe_id = (gint) g_ascii_strtoll ((gchar *)recipe_id, NULL, 0);
    g_free (recipe_id);

    // If the port number was recorded use it.
    xmlChar *port = xmlGetNoNsProp(app_data->recipe_node_ptr, (xmlChar *)"port");
    if (port) {
        app_data->port = (gint) g_ascii_strtoll ((gchar *)port, NULL, 0);
    }

    // find task nodes
    xmlXPathObjectPtr node_ptrs = get_node_set (app_data->xml_doc, task_xpath);
    if (!node_ptrs) {
        g_printerr ("No <task> element(s) in %s\n", filename);
        g_free (filename);
        xmlXPathFreeObject (node_ptrs);
        xmlFreeDoc(app_data->xml_doc);
        return;
    }
    // record each task in a hash table
    app_data->tasks = g_hash_table_new (g_str_hash, g_str_equal);
    parse_task_nodes (node_ptrs->nodesetval, app_data->tasks);
    xmlXPathFreeObject (node_ptrs);
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
    gchar* results_filename;
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

    if (!g_spawn_command_line_sync(cmdline, &std_out, &std_err, &exitstat, &gerror)) {
        g_printerr("cannot spawn '%s': %s\n", cmdline, gerror->message);
        g_error_free(gerror);
        goto cleanup;
    }
    if (exitstat != 0) {
        g_printerr("xsltproc error: %s\n", std_err);
        goto cleanup;
    }

    results_filename = g_build_filename (run_dir, "index.html", NULL);
    results_fd = g_open(results_filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (results_fd == -1) {
        g_printerr("Error opening %s for writing\n", results_filename);
        goto cleanup;
    }
    length = strlen(std_out);
    results_len = write(results_fd, std_out, length);
    if(!g_close(results_fd, &gerror)) {
        g_printerr("cannot close %s: %s\n", results_filename, gerror->message);
        g_error_free(gerror);
        goto cleanup;
    }
    if (results_len != length) {
        g_printerr("Error writing to %s\n", results_filename);
    }

cleanup:
    if (contents != NULL)
        g_free (contents);
    if (cmdline != NULL)
        g_free (cmdline);
    if (std_out != NULL)
        g_free (std_out);
    if (std_err != NULL)
        g_free (std_err);
}

int main(int argc, char *argv[]) {

    SoupServer *server;
    SoupAddress *addr;
    SoupAddressFamily address_family;

    gchar *remote = "http://localhost:8081"; // Replace with a unix socket proxy so no network is required
                                             // when run from localhost.
    gchar *job = NULL;
    gboolean ipv6 = FALSE;

    AppData *app_data = g_slice_new0 (AppData);
    app_data->body = g_string_new (NULL);

    init_result_hash (app_data);

    GOptionEntry entries[] = {
        {"remote", 's', 0, G_OPTION_ARG_STRING, &remote,
            "Remote machine to connect to", "URL" },
        {"ipv6", '6', 0, G_OPTION_ARG_NONE, &ipv6,
            "Use IPV6 for communication", NULL },
        {"port", 'p', 0, G_OPTION_ARG_INT, &app_data->port,
            "Specify the port to listen on", "PORT" },
        { "job", 'j', 0, G_OPTION_ARG_STRING, &job,
            "Run job from file", "FILE" },
        { "run", 'r', 0, G_OPTION_ARG_STRING, &app_data->run_dir,
            "Continue interrupted job from DIR", "DIR" },
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, callback_parse_verbose,
            "Increase verbosity, up to three times.", NULL },
        { NULL }
    };
    GOptionGroup *option_group = g_option_group_new ("main",
                                                    "Application Options",
                                                    "Various application related options",
                                                    app_data, NULL);
    GOptionContext *context = g_option_context_new(NULL);

    g_option_context_set_summary (context,
            "Test harness for Beaker. Runs tasks according to a job \n"
            "and collects their results.");
    g_option_group_add_entries(option_group, entries);
    g_option_context_set_main_group (context, option_group);

    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &app_data->error);
    g_option_context_free(context);

    // Setup soup session to talk to restraintd
    app_data->remote_uri = soup_uri_new (remote);

    if (ipv6) {
        address_family = SOUP_ADDRESS_FAMILY_IPV6;
    } else {
        address_family = SOUP_ADDRESS_FAMILY_IPV4;
    }

    SoupAddress *address = soup_address_new_any (address_family,
                                                   SOUP_ADDRESS_ANY_PORT);

    session = soup_session_new_with_options("local-address", address, NULL);

    if (job) {
        // if template job is passed in use it to generate our job
        app_data->run_dir = copy_job_as_template (job);
    }

    if (app_data->run_dir && app_data->port) {
        g_printerr ("You can't specify both run_dir and port\n");
        g_printerr ("Try %s --help\n", argv[0]);
        goto cleanup;
    }
    if (!parse_succeeded || !app_data->run_dir) {
        g_printerr("Try %s --help\n", argv[0]);
        goto cleanup;
    }

    // Read in run_dir/job.xml
    parse_new_job (app_data);

    // If all tasks are finished then quit.
    if (tasks_finished (app_data->tasks)) {
        g_printerr ("All tasks are finished\n");
        goto cleanup;
    }

    // ask restraintd what ip address we connected with.
    SoupURI *control_uri = soup_uri_new_with_base (app_data->remote_uri, "address");
    SoupMessage *address_msg = soup_message_new_from_uri("GET", control_uri);
    soup_uri_free (control_uri);
    soup_session_send_message (session, address_msg);
    if (!SOUP_STATUS_IS_SUCCESSFUL(address_msg->status_code)) {
        g_printerr ("%s\n", address_msg->reason_phrase);
        goto cleanup;
    }

    // Run a REST Server to gather results
    if (app_data->port) {
        addr = soup_address_new_any (address_family,
                                     app_data->port);
    } else {
        addr = soup_address_new_any (address_family,
                                     SOUP_ADDRESS_ANY_PORT);
    }
    server = soup_server_new (SOUP_SERVER_INTERFACE, addr, NULL);
    app_data->port = soup_server_get_port (server);
    if (!server) {
        g_printerr ("Unable to bind to server port %d\n", app_data->port);
        exit (1);
    }

    // Construct the url where we will serve from.
    gchar *host = g_strdup(soup_message_headers_get_one (address_msg->response_headers, "Address"));
    SoupURI *proxy_uri = soup_uri_new(NULL);
    soup_uri_set_scheme (proxy_uri, "http");
    soup_uri_set_host (proxy_uri, host);
    soup_uri_set_port (proxy_uri, app_data->port);
    soup_uri_set_path (proxy_uri, "/");
    app_data->address = soup_uri_to_string (proxy_uri, FALSE);
    soup_uri_free (proxy_uri);
    g_free (host);
    g_object_unref (address_msg);

    // Record the port with the job.xml
    gchar *port_str = g_strdup_printf ("%d", app_data->port);
    xmlSetProp (app_data->recipe_node_ptr, (xmlChar *)"port", (xmlChar *) port_str);
    g_free (port_str);
    gchar *filename = g_build_filename (app_data->run_dir, "job.xml", NULL);
    put_doc (app_data->xml_doc, filename);
    g_free (filename);

    gchar *recipe_path = g_strdup_printf ("/recipes/%d", app_data->recipe_id);
    soup_server_add_handler (server, recipe_path,
                             recipe_callback, app_data, NULL);
    g_free (recipe_path);
    gchar *task_path = g_strdup_printf ("/recipes/%d/tasks", app_data->recipe_id);
    soup_server_add_handler (server, task_path,
                             task_callback, app_data, NULL);
    g_free (task_path);
    soup_server_run_async (server);

    // Request to run the recipe.
    g_idle_add_full (G_PRIORITY_LOW,
                         run_recipe_handler,
                         app_data,
                         NULL);

    // Create and enter the main loop
    app_data->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(app_data->loop);

    // We're done.
    xmlFreeDoc(app_data->xml_doc);
    xmlCleanupParser();

    // convert job.xml to index.html
    pretty_results(app_data->run_dir);

cleanup:
    if (app_data->error) {
        g_printerr("%s [%s, %d]\n", app_data->error->message,
                g_quark_to_string(app_data->error->domain), app_data->error->code);
        return app_data->error->code;
    } else {
        return EXIT_SUCCESS;
    }
}
