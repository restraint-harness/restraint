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

#include <glib.h>
#include <libsoup/soup.h>
#include <time.h>
#include <stdint.h>
#include <json.h>
#include "message.h"

static GQueue *message_queue = NULL;
static gboolean queue_active = FALSE;

static gboolean message_handler (gpointer data);

static gboolean
message_finish (gpointer user_data)
{
    MessageData *message_data = (MessageData *) user_data;
    message_data->finish_callback (message_data->session,
                                   message_data->msg,
                                   message_data->user_data);
    g_object_unref (message_data->msg);
    return FALSE;
}

static void
message_destroy (gpointer user_data)
{
    MessageData *message_data = (MessageData *) user_data;
    g_slice_free (MessageData, message_data);
}

static void
message_complete (SoupSession *sesison, SoupMessage *msg, gpointer user_data)
{
    //g_print ("message_complete->enter\n");
    MessageData *message_data = (MessageData *) user_data;
    static gint delay = 2;

    if (SOUP_STATUS_IS_SUCCESSFUL (message_data->msg->status_code) ||
        SOUP_STATUS_IS_CLIENT_ERROR (message_data->msg->status_code)) {
        delay = 2;
        if (message_data->finish_callback) {
            message_data->finish_callback (message_data->session,
                                           message_data->msg,
                                           message_data->user_data);
        }
        g_slice_free (MessageData, message_data);

        //g_print ("message_complete->add message_handler\n");
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         message_handler,
                         NULL,
                         NULL);
    } else {
        // failed to send message
        delay = (gint ) (delay * 1.5);
        // Don't wait more than 10 minutes between retries.
        if (delay > 625) {
            delay = 625;
        }
        gchar *uri = soup_uri_to_string(soup_message_get_uri(message_data->msg), TRUE);
        g_warning("%s: Unable to send %s, delaying %d seconds..",
                  message_data->msg->reason_phrase,
                  uri,
                  delay);

        g_free(uri);
        // push it back onto the queue.
        (void)g_object_ref (message_data->msg);
        g_queue_push_head (message_queue, message_data);
        //g_print ("message_complete->add delay_handler\n");
        g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE,
                                    delay,
                                    message_handler,
                                    NULL,
                                    NULL);
    }
    //g_print ("message_complete->exit\n");
}

static gboolean
message_handler (gpointer data)
{
    //g_print ("message_handler->enter\n");
    // Make sure we have something in the queue.
    if (g_queue_is_empty (message_queue)) {
        queue_active = FALSE;
    } else {
        MessageData *message_data = g_queue_pop_head (message_queue);
        soup_session_queue_message (message_data->session,
                                    message_data->msg,
                                    message_complete,
                                    message_data);
    }
    //g_print ("message_handler->exit\n");
    return FALSE;
}

void
restraint_queue_message (SoupSession *session,
                         SoupMessage *msg,
                         gpointer msg_data,
                         MessageFinishCallback finish_callback,
                         GCancellable *cancellable,
                         gpointer user_data)
{
    //g_print ("restraint_queue_message->enter\n");
    MessageData *message_data;
    message_data = g_slice_new0 (MessageData);
    message_data->msg = msg;
    message_data->session = session;
    message_data->user_data = user_data;
    message_data->finish_callback = finish_callback;

    // Initialize the queue if needed
    if (!message_queue) {
        message_queue = g_queue_new ();
    }

    // push the message onto the queue.
    g_queue_push_tail (message_queue, message_data);

    // Add the message handler to the main loop if it isn't running already.
    if (!queue_active) {
        //g_print ("restraint_queue_message->add message_handler\n");
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         message_handler,
                         NULL,
                         NULL);
        queue_active = TRUE;
    }
    //g_print ("restraint_queue_message->exit\n");
}

static void
ghash_append_json_header (gpointer data_name, gpointer data_value, gpointer user_data)
{
    const char *name = (const char *) data_name;
    const char *value = (const char *) data_value;
    json_object *headers = (json_object *) user_data;
    json_object_object_add (headers, name, json_object_new_string(value));
}

static void
soup_append_json_header (const char *name, const char *value, gpointer user_data)
{
    json_object *headers = (json_object *) user_data;
    json_object_object_add (headers, name, json_object_new_string(value));
}

void
restraint_close_message (gpointer msg_data)
{
        exit(0);
}

void
restraint_stdout_message (SoupSession *session,
                          SoupMessage *msg,
                          gpointer msg_data,
                          MessageFinishCallback finish_callback,
                          GCancellable *cancellable,
                          gpointer user_data)
{
    ClientData *client_data = (ClientData *) msg_data;
    time_t result;
    result = time(NULL);
    static time_t transaction_id = 0;
    // calculate the transaction id. base it off of epoch
    // incase the host reboots we shouldn't collide
    if (transaction_id == 0) {
        transaction_id = result;
    }
    MessageData *message_data;
    message_data = g_slice_new0 (MessageData);
    message_data->msg = msg;
    message_data->user_data = user_data;
    message_data->finish_callback = finish_callback;

    if (client_data != NULL) {
        struct json_object *jobj;
        struct json_object *jobj_headers;
        struct json_object *jobj_body;

        jobj = json_object_new_object();
        jobj_headers = json_object_new_object();
        json_object_object_add(jobj, "headers", jobj_headers);

        SoupURI *uri = soup_message_get_uri (msg);

        soup_message_headers_foreach (msg->request_headers, soup_append_json_header,
                                      jobj_headers);
        // if we are doing a POST transaction
        // increment transaction_id and add it to headers
        // populate Location header in msg->reponse_headers
        const gchar *path = soup_uri_get_path (uri);
        if (g_strcmp0 (msg->method, "POST") == 0) {
            gchar *transaction_id_string = g_strdup_printf("%jd", (intmax_t) transaction_id);
            json_object_object_add (jobj_headers,
                            "transaction-id",
                            json_object_new_string(transaction_id_string));

            gchar *location_url = g_strdup_printf ("%s%s", path, transaction_id_string);
            soup_message_headers_append (msg->response_headers, "Location", location_url);
            g_free (transaction_id_string);
            g_free (location_url);

            transaction_id++;
        }
        soup_message_set_status (msg, SOUP_STATUS_OK);
        SoupBuffer *request = soup_message_body_flatten (msg->request_body);
        json_object_object_add (jobj_headers, "rstrnt-path", json_object_new_string(path));
        json_object_object_add (jobj_headers, "rstrnt-method", json_object_new_string(msg->method));
        json_object_object_add (jobj_headers, "body-length", json_object_new_int(request->length));

        if (g_strrstr (path, "/logs/")) {
            // base64 encode the body for logs
            gchar *encoded = g_base64_encode ((unsigned char*) request->data, request->length);
            jobj_body = json_object_new_string (encoded);
            g_free (encoded);
        } else {
            GHashTable *table;

            table = soup_form_decode (request->data);
            jobj_body = json_object_new_object ();
            // translate form_data into json body->keys->values
            g_hash_table_foreach (table, ghash_append_json_header, jobj_body);
            g_hash_table_destroy (table);
        }

        json_object_object_add (jobj, "body", jobj_body);

        // Print json_root to STDOUT
        const gchar *json_text = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN);
        //g_print ("%010lu", strlen(json_text));
        g_print ("%s\n",json_text);

        soup_buffer_free (request);
        json_object_put (jobj); // Delete the json object
    }

    if (finish_callback) {
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         message_finish,
                         message_data,
                         message_destroy);
    } else {
        g_object_unref (msg);
        message_destroy (message_data);
    }
}
