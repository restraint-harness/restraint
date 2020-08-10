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
