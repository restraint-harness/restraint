
#include <glib.h>

#include "task.h"
#include "recipe.h"
#include "expect_http.h"

SoupSession *soup_session;

static void test_package_installation_failure_aborts(void) {
    soup_session = soup_session_new();
    ExpectHttpServer *expect_http = expect_http_start();
    ExpectHttpRequest request = { "POST", "/recipes/123/tasks/456/status",
            "status=Aborted&message=While+installing+package+"
            "beaker%2Ddistribution%2Dinstall%2Dfail%3A+"
            "Child+process+exited+with+code+1",
            204 };
    expect_http_add_request(expect_http, &request);
    Task task = {
        "456",
        NULL,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "/mnt/tests/distribution/install",
        TASK_FETCH_INSTALL_PACKAGE,
        { .package_name = "beaker-distribution-install-fail" },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        TRUE,
        "make run",
        0,
    };
    restraint_task_run(&task);
    expect_http_finish(expect_http);
    soup_uri_free(task.task_uri);
}

static void test_fetch_git(void) {
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        g_dir_make_tmp("restraint_test_task_XXXXXX", NULL),
        TASK_FETCH_UNPACK,
        { .url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git") },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        FALSE,
        NULL,
        0,
    };
    restraint_task_run(&task);

    // assert it's on disk
    GFile *base = g_file_new_for_path(task.path);
    GFile *file = g_file_get_child(base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    g_object_unref(base);

    // assert this is not rhts_compat task
    g_assert(task.rhts_compat == FALSE);

    soup_uri_free(task.task_uri);
    g_free(task.path);
    g_strfreev(task.entry_point);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_generate(void) {
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        g_dir_make_tmp("restraint_test_task_XXXXXX", NULL),
        TASK_FETCH_UNPACK,
        { .url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/testinfo_generate") },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        FALSE,
        NULL,
        0,
    };
    restraint_task_run(&task);

    // assert it's on disk
    GFile *base = g_file_new_for_path(task.path);
    GFile *file = g_file_get_child(base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_object_unref(file);
    g_object_unref(base);

    // assert this is an rhts_compat task
    g_assert(task.rhts_compat == TRUE);

    soup_uri_free(task.task_uri);
    g_free(task.path);
    g_strfreev(task.entry_point);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    soup_uri_free(task.fetch.url);
}

static void test_fetch_git_negative(void) {
    // test-data/git-remote/repo2 has uploadarch=false in .git/config
    ExpectHttpServer *expect_http = expect_http_start();
    ExpectHttpRequest request = { "POST", "/recipes/123/tasks/456/status",
            "status=Aborted&message=Error+from+remote+git+daemon%3A+"
            "access+denied+or+repository+not+exported%3A+%2Frepo2",
            204 };
    expect_http_add_request(expect_http, &request);

    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        g_dir_make_tmp("restraint_test_task_XXXXXX", NULL),
        TASK_FETCH_UNPACK,
        { .url = soup_uri_new("git://localhost/repo2?master#restraint/sanity/fetch_git") },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        FALSE,
        NULL,
        0,
    };
    restraint_task_run(&task);

    expect_http_finish(expect_http);
    GFile *base = g_file_new_for_path(task.path);
    GFile *file = g_file_get_child(base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) == FALSE);
    g_object_unref(file);
    g_object_unref(base);

    soup_uri_free(task.task_uri);
    g_free(task.path);
    soup_uri_free(task.fetch.url);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/package_installation_failure_aborts",
            test_package_installation_failure_aborts);
    g_test_add_func("/task/fetch_git", test_fetch_git);
    g_test_add_func("/task/fetch_git/negative", test_fetch_git_negative);
    g_test_add_func("/task/testinfo/generate", test_testinfo_generate);
    return g_test_run();
}
