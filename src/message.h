typedef void (*MessageFinishCallback)	(SoupSession *session,
                                         SoupMessage *msg,
                                         gpointer	user_data);

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
                              MessageFinishCallback finish_callback,
                              gpointer user_data);
