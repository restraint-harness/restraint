
#include <glib.h>
#include <gio/gio.h>

#include "task.h"
#include "metadata.h"

SoupSession *soup_session;

static void test_testinfo_dependencies(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/dependencies/requires",
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
    gchar *dependency;

    restraint_metadata_update(&task, &error);

    dependency = g_list_nth_data(task.dependencies, 0);
    g_assert_cmpstr(dependency, ==, "lib-virt");
    dependency = g_list_nth_data(task.dependencies, 1);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_list_nth_data(task.dependencies, 2);
    g_assert_cmpstr(dependency, ==, "postgresql");
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_dependencies_rhtsrequires(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/dependencies/rhtsrequires",
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
    gchar *dependency;

    restraint_metadata_update(&task, &error);

    dependency = g_list_nth_data(task.dependencies, 0);
    g_assert_cmpstr(dependency, ==, "lib-virt");
    dependency = g_list_nth_data(task.dependencies, 1);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_list_nth_data(task.dependencies, 2);
    g_assert_cmpstr(dependency, ==, "postgresql");
    dependency = g_list_nth_data(task.dependencies, 12);
    g_assert_cmpstr(dependency, ==, "foo");
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_day(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/day",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 60 * 24 * 2);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_hour(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/hour",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 60 * 10);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_invalid(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/invalid",
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

    restraint_metadata_update(&task, &error);
    g_print(error->message);
    g_assert_cmpstr(error->message, ==, "Task 456: Unrecognised time unit 'G'");
    g_error_free(error);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_minute(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/minute",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 20);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_second(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/second",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 200);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_testinfo_testtime_default(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_testinfo/testtime/default",
        TASK_FETCH_UNPACK,
        { .url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git") },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        FALSE,
        NULL,
        DEFAULT_MAX_TIME,
    };

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, DEFAULT_MAX_TIME);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_dependencies(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/dependencies",
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
    gchar *dependency;

    restraint_metadata_update(&task, &error);

    dependency = g_list_nth_data(task.dependencies, 0);
    g_assert_cmpstr(dependency, ==, "lib-virt");
    dependency = g_list_nth_data(task.dependencies, 1);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_list_nth_data(task.dependencies, 2);
    g_assert_cmpstr(dependency, ==, "postgresql");
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_day(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/day",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 60 * 24 * 2);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_hour(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/hour",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 60 * 10);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_invalid(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/invalid",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpstr(error->message, ==, "Task 456: Unrecognised time unit 'G'");
    g_error_free(error);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_minute(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/minute",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 60 * 20);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_second(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/second",
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

    restraint_metadata_update(&task, &error);
    g_assert_cmpuint(task.max_time, ==, 200);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

static void test_metadata_testtime_default(void) {
    GError *error = NULL;
    Recipe recipe = {"1","1","1","RHEL-6.0","RedHatEnterpriseLinux6","Server","x86_64",NULL,NULL,NULL};
    Task task = {
        "456",
        &recipe,
        soup_uri_new("http://localhost:8000/recipes/123/tasks/456/"),
        "/distribution/install",
        "test-data/parse_metadata/testtime/default",
        TASK_FETCH_UNPACK,
        { .url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git") },
        NULL,
        NULL,
        FALSE,
        FALSE,
        NULL,
        FALSE,
        NULL,
        DEFAULT_MAX_TIME,
    };

    restraint_metadata_update(&task, &error);
    g_assert_cmpstr(error->message, ==, "Task 456: Key file does not have key 'max_time'");
    g_error_free(error);
    g_assert_cmpuint(task.max_time, ==, DEFAULT_MAX_TIME);
    g_list_free_full(task.dependencies, (GDestroyNotify) g_free);
    g_free(task.entry_point);
    soup_uri_free(task.task_uri);
    soup_uri_free(task.fetch.url);
}

int main(int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/testindo.desc/testtime/day", test_testinfo_testtime_day);
    g_test_add_func("/testindo.desc/testtime/default", test_testinfo_testtime_default);
    g_test_add_func("/testindo.desc/testtime/hour", test_testinfo_testtime_hour);
    g_test_add_func("/testindo.desc/testtime/invalid", test_testinfo_testtime_invalid);
    g_test_add_func("/testindo.desc/testtime/minute", test_testinfo_testtime_minute);
    g_test_add_func("/testindo.desc/testtime/second", test_testinfo_testtime_second);
    g_test_add_func("/testinfo.desc/dependencies", test_testinfo_dependencies);
    g_test_add_func("/testinfo.desc/dependencies/rhtsrequires", test_testinfo_dependencies_rhtsrequires);
    g_test_add_func("/metadata/testtime/day", test_metadata_testtime_day);
    g_test_add_func("/metadata/testtime/default", test_metadata_testtime_default);
    g_test_add_func("/metadata/testtime/hour", test_metadata_testtime_hour);
    g_test_add_func("/metadata/testtime/invalid", test_metadata_testtime_invalid);
    g_test_add_func("/metadata/testtime/minute", test_metadata_testtime_minute);
    g_test_add_func("/metadata/testtime/second", test_metadata_testtime_second);
    g_test_add_func("/metadata/dependencies", test_metadata_dependencies);
    return g_test_run();
}
