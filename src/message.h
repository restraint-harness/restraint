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

#ifndef _RESTRAINT_MESSAGE_H
#define _RESTRAINT_MESSAGE_H

typedef void (*MessageFinishCallback)   (SoupSession *session,
                                         SoupMessage *msg,
                                         gpointer user_data);

typedef void (*QueueMessage)	(SoupSession	*session,
				 SoupMessage	*message,
				 gpointer	message_data,
				 MessageFinishCallback	callback,
				 GCancellable   *cancellable,
				 gpointer	user_data);
typedef void (*CloseMessage)	(gpointer	message_data);

typedef struct {
    // Session to use
    SoupSession *session;
    // message to send
    SoupMessage *msg;
    // user data to pass to finish_callback
    gpointer user_data;
    // when message is successfully sent run this callback
    MessageFinishCallback finish_callback;
    // Delay requeue by this many seconds.
    guint delay;
} MessageData;

void restraint_queue_message (SoupSession *session,
                              SoupMessage *msg,
                              gpointer msg_data,
                              MessageFinishCallback finish_callback,
                              GCancellable *cancellable,
                              gpointer user_data);
#endif
