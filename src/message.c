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
#include "message.h"

static GQueue *message_queue = NULL;
static gboolean queue_active = FALSE;

static gboolean message_handler (gpointer data);

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
        g_object_ref (message_data->msg);
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
