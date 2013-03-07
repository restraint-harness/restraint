
#include <glib.h>
#include <gio/gio.h>

#include "task.h"
#include "recipe.h"

static void test_parse_from_file(void) {
    GFile *file = g_file_new_for_path("test-data/recipe.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_no_error(error);
    g_assert(recipe != NULL);

    g_assert_cmpstr(recipe->recipe_id, ==, "796557");
    g_assert_cmpuint(g_list_length(recipe->tasks), ==, 4);

    Task *task = g_list_nth_data(recipe->tasks, 0);
    g_assert_cmpstr(task->task_id, ==, "10722631");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722631/"));
    g_assert_cmpstr(task->name, ==, "/distribution/install");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==, "beaker-distribution-install");
    g_assert_cmpuint(task->started, ==, TRUE);
    g_assert_cmpuint(task->finished, ==, TRUE);

    task = g_list_nth_data(recipe->tasks, 1);
    g_assert_cmpstr(task->task_id, ==, "10722632");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722632/"));
    g_assert_cmpstr(task->name, ==, "/distribution/kernelinstall");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-kernelinstall");
    g_assert_cmpuint(task->started, ==, TRUE);
    g_assert_cmpuint(task->finished, ==, FALSE);

    task = g_list_nth_data(recipe->tasks, 2);
    g_assert_cmpstr(task->task_id, ==, "10722633");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722633/"));
    g_assert_cmpstr(task->name, ==, "/distribution/virt/install");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-virt-install");
    g_assert_cmpuint(task->started, ==, FALSE);
    g_assert_cmpuint(task->finished, ==, FALSE);

    task = g_list_nth_data(recipe->tasks, 3);
    g_assert_cmpstr(task->task_id, ==, "10722634");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722634/"));
    g_assert_cmpstr(task->name, ==, "/distribution/virt/start");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-virt-start");
    g_assert_cmpuint(task->started, ==, FALSE);
    g_assert_cmpuint(task->finished, ==, FALSE);

    restraint_recipe_free(recipe);
}

int main(int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/recipe/parse/from_file", test_parse_from_file);
    return g_test_run();
}
