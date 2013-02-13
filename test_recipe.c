
#include <glib.h>
#include <gio/gio.h>

#include "recipe.h"

static void test_parse_from_file(void) {
    GFile *file = g_file_new_for_path("test-data/recipe.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_parse_recipe(file, &error);
    g_object_unref(file);
    g_assert_no_error(error);
    g_assert(recipe != NULL);
    g_assert_cmpstr(recipe->recipe_id, ==, "796557");
    g_assert_cmpuint(g_list_length(recipe->tasks), ==, 4);
    RecipeTask *task = g_list_nth_data(recipe->tasks, 0);
    g_assert_cmpstr(task->task_id, ==, "10722631");
    g_assert_cmpstr(task->name, ==, "/distribution/install");
    g_assert_cmpuint(task->fetch_method, ==, FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==, "beaker-distribution-install");
    restraint_free_recipe(recipe);
}

int main(int argc, char *argv[]) {
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/recipe/parse/from_file", test_parse_from_file);
    return g_test_run();
}
