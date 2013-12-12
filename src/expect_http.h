
#ifndef _RESTRAINT_EXPECT_HTTP_H
#define _RESTRAINT_EXPECT_HTTP_H

/*
 * For tests: runs a dummy HTTP server in a separate thread. The server expects 
 * to receive the given series of requests. Anything else is a failure.
 */

#include <glib.h>
#include <libsoup/soup.h>

typedef struct {
    GThread *thread;
    GMutex mutex;
    SoupServer *server;
    GQueue *expected_requests;
} ExpectHttpServer;

// Use only static strings in this struct!
typedef struct {
    const gchar *method;
    const gchar *path;
    const gchar *body;
    guint response_status;
} ExpectHttpRequest;

ExpectHttpServer *expect_http_start(void);
void expect_http_add_request(ExpectHttpServer *expect_http,
        const ExpectHttpRequest *request);
void expect_http_finish(ExpectHttpServer *expect_http);

#endif
