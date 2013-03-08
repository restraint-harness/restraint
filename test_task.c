
#include <glib.h>

#include "task.h"
#include "expect_http.h"

static void test_package_installation_failure_aborts(void) {
    ExpectHttpServer *expect_http = expect_http_start();
    ExpectHttpRequest request = { "POST", "/recipes/123/tasks/456/status",
            "status=Aborted&message=While+installing+package+"
            "beaker%2Ddistribution%2Dinstall%2Dfail%3A+"
            "Child+process+exited+with+code+1",
            204 };
    expect_http_add_request(expect_http, &request);
    Task task = {
        "123",
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        TASK_FETCH_INSTALL_PACKAGE,
        { .package_name = "beaker-distribution-install-fail" },
        FALSE,
        FALSE,
    };
    restraint_task_run(&task);
    expect_http_finish(expect_http);
    soup_uri_free(task.task_uri);
}

int main(int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/package_installation_failure_aborts",
            test_package_installation_failure_aborts);
    return g_test_run();
}
