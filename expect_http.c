
#include "expect_http.h"

static void expect_http_handler(__attribute__((unused)) SoupServer *server,
        SoupMessage *msg, const char *path,
        __attribute__((unused)) GHashTable *query,
        __attribute__((unused)) SoupClientContext *client,
        gpointer data) {
    ExpectHttpServer *expect_http = (ExpectHttpServer *)data;

    g_mutex_lock(&expect_http->mutex);
    if (g_queue_is_empty(expect_http->expected_requests))
        g_error("Unexpected HTTP request");
    ExpectHttpRequest *request = (ExpectHttpRequest *)g_queue_pop_head(
            expect_http->expected_requests);
    g_mutex_unlock(&expect_http->mutex);

    g_assert_cmpstr(msg->method, ==, request->method);
    g_assert_cmpstr(path, ==, request->path);
    g_assert_cmpstr(msg->request_body->data, ==, request->body);
    soup_message_set_status(msg, request->response_status);

    g_slice_free(ExpectHttpRequest, request);
}

ExpectHttpServer *expect_http_start(void) {
    ExpectHttpServer *expect_http = g_slice_new0(ExpectHttpServer);
    g_assert(expect_http != NULL);

    g_mutex_init(&expect_http->mutex);

    expect_http->server = soup_server_new(SOUP_SERVER_PORT, 8000, NULL);
    g_assert(expect_http->server != NULL);
    soup_server_add_handler(expect_http->server, NULL,
            expect_http_handler, expect_http, NULL);

    expect_http->expected_requests = g_queue_new();

    expect_http->thread = g_thread_new("expect-http-server",
            (GThreadFunc) soup_server_run, expect_http->server);
    g_assert(expect_http->thread != NULL);
    return expect_http;
}

void expect_http_add_request(ExpectHttpServer *expect_http,
        const ExpectHttpRequest *request) {
    ExpectHttpRequest *copy = g_slice_new0(ExpectHttpRequest);
    *copy = *request;
    g_mutex_lock(&expect_http->mutex);
    g_queue_push_tail(expect_http->expected_requests, copy);
    g_mutex_unlock(&expect_http->mutex);
}

void expect_http_finish(ExpectHttpServer *expect_http) {
    g_mutex_lock(&expect_http->mutex);
    soup_server_quit(expect_http->server);
    g_mutex_unlock(&expect_http->mutex);

    g_thread_join(expect_http->thread);

    if (!g_queue_is_empty(expect_http->expected_requests))
        g_error("Not all expected HTTP requests were received");
    g_mutex_clear(&expect_http->mutex);
    g_object_unref(expect_http->server);
    g_queue_free(expect_http->expected_requests);
    g_slice_free(ExpectHttpServer, expect_http);
}
