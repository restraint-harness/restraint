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


#include <glib.h>
#include <gio/gio.h>

#include "task.h"
#include "param.h"
#include "role.h"
#include "recipe.h"

SoupSession *soup_session;

static void test_parse_traditional(void) {
    // This is a "traditional" recipe, where all tasks come from the task library.
    GFile *file = g_file_new_for_path("test-data/recipe.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_no_error(error);
    g_assert(recipe != NULL);

    g_assert_cmpstr(recipe->job_id, ==, "379231");
    g_assert_cmpstr(recipe->recipe_set_id, ==, "648468");
    g_assert_cmpstr(recipe->recipe_id, ==, "796557");
    g_assert_cmpuint(g_list_length(recipe->tasks), ==, 4);
    g_assert_cmpstr(recipe->osarch, ==, "x86_64");
    g_assert_cmpstr(recipe->osdistro, ==, "RHEL5-Server-U8");
    g_assert_cmpstr(recipe->osmajor, ==, "RedHatEnterpriseLinuxServer5");
    g_assert_cmpstr(recipe->osvariant, ==, "");
    g_assert_cmpuint(g_list_length(recipe->roles), ==, 1);
    Role *role = g_list_nth_data(recipe->roles, 0);
    g_assert_cmpstr(role->value, ==, "SERVERS");
    g_assert_cmpstr(role->systems, ==, "hostname1.example.com hostname2.example.com");
    g_assert_cmpuint(g_list_length(recipe->params), ==, 1);
    Param *param = g_list_nth_data(recipe->params, 0);
    g_assert_cmpstr(param->name, ==, "GLOBAL");
    g_assert_cmpstr(param->value, ==, "foo");

    Task *task = g_list_nth_data(recipe->tasks, 0);
    g_assert_cmpstr(task->task_id, ==, "10722631");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722631/"));
    g_assert_cmpstr(task->path, ==, "/mnt/tests/distribution/install");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==, "beaker-distribution-install");
    g_assert_cmpuint(task->started, ==, TRUE);
    g_assert_cmpuint(task->finished, ==, TRUE);
    g_assert_cmpuint(g_list_length(task->params), ==, 0);

    task = g_list_nth_data(recipe->tasks, 1);
    g_assert_cmpstr(task->task_id, ==, "10722632");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722632/"));
    g_assert_cmpstr(task->path, ==, "/mnt/tests/distribution/kernelinstall");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-kernelinstall");
    g_assert_cmpuint(task->started, ==, TRUE);
    g_assert_cmpuint(task->finished, ==, FALSE);
    g_assert_cmpuint(g_list_length(task->params), ==, 3);
    param = g_list_nth_data(task->params, 0);
    g_assert_cmpstr(param->name, ==, "KERNELARGNAME");
    g_assert_cmpstr(param->value, ==, "kernel");
    param = g_list_nth_data(task->params, 1);
    g_assert_cmpstr(param->name, ==, "KERNELARGVARIANT");
    g_assert_cmpstr(param->value, ==, "xen");
    param = g_list_nth_data(task->params, 2);
    g_assert_cmpstr(param->name, ==, "KERNELARGVERSION");
    g_assert_cmpstr(param->value, ==, "2.6.18-308.el5");

    task = g_list_nth_data(recipe->tasks, 2);
    g_assert_cmpstr(task->task_id, ==, "10722633");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722633/"));
    g_assert_cmpstr(task->path, ==, "/mnt/tests/distribution/virt/install");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-virt-install");
    g_assert_cmpuint(task->started, ==, FALSE);
    g_assert_cmpuint(task->finished, ==, FALSE);
    g_assert_cmpuint(g_list_length(task->params), ==, 0);

    task = g_list_nth_data(recipe->tasks, 3);
    g_assert_cmpstr(task->task_id, ==, "10722634");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722634/"));
    g_assert_cmpstr(task->path, ==, "/mnt/tests/distribution/virt/start");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_INSTALL_PACKAGE);
    g_assert_cmpstr(task->fetch.package_name, ==,
            "distribution-distribution-virt-start");
    g_assert_cmpuint(task->started, ==, FALSE);
    g_assert_cmpuint(task->finished, ==, FALSE);
    g_assert_cmpuint(g_list_length(task->params), ==, 0);

    restraint_recipe_free(recipe);
}

static void test_parse_git(void) {
    // Recipe with a task fetched from git.
    GFile *file = g_file_new_for_path("test-data/recipe-git.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_no_error(error);
    g_assert(recipe != NULL);

    g_assert_cmpuint(g_list_length(recipe->tasks), ==, 1);

    Task *task = g_list_nth_data(recipe->tasks, 0);
    g_assert_cmpstr(task->task_id, ==, "10722631");
    g_assert(g_str_has_suffix(soup_uri_get_path(task->task_uri),
            "/tasks/10722631/"));
    g_assert_cmpstr(task->path, ==,
            "/mnt/tests/git.beaker-project.org/beaker/Tasks/distribution/install");
    g_assert_cmpuint(task->fetch_method, ==, TASK_FETCH_UNPACK);
    gchar *fetch_url_string = soup_uri_to_string(task->fetch.url, FALSE);
    g_assert_cmpstr(fetch_url_string, ==,
            "git://git.beaker-project.org/beaker#Tasks/distribution/install");
    g_free(fetch_url_string);
    g_assert_cmpuint(task->started, ==, TRUE);
    g_assert_cmpuint(task->finished, ==, TRUE);

    restraint_recipe_free(recipe);
}

static void test_parse_bad_recipe_params(void) {
    // Recipe with a bad recipe param value
    GFile *file = g_file_new_for_path("test-data/recipe-git-bad-recipe-params.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_cmpstr(error->message, ==, "Recipe 796557 has 'param' element without 'name' attribute");
    g_error_free(error);
    g_assert(recipe == NULL);
}

static void test_parse_bad_task_params(void) {
    // Recipe with a bad task param value
    GFile *file = g_file_new_for_path("test-data/recipe-git-bad-task-params.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_cmpstr(error->message, ==, "Task 10722631 has 'param' element without 'value' attribute");
    g_error_free(error);
    g_assert(recipe == NULL);
}

static void test_parse_bad_recipe_roles(void) {
    // Recipe with a bad recipe role->system value
    GFile *file = g_file_new_for_path("test-data/recipe-git-bad-recipe-roles.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_cmpstr(error->message, ==, "Recipe 796557 has 'system' element without 'value' attribute");
    g_error_free(error);
    g_assert(recipe == NULL);
}

static void test_parse_bad_task_roles(void) {
    // Recipe with a bad task role value
    GFile *file = g_file_new_for_path("test-data/recipe-git-bad-task-roles.xml");
    g_assert(file != NULL);
    GError *error = NULL;
    Recipe *recipe = restraint_recipe_new_from_xml(file, &error);
    g_object_unref(file);
    g_assert_cmpstr(error->message, ==, "Task 10722631 has 'role' element without 'value' attribute");
    g_error_free(error);
    g_assert(recipe == NULL);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/recipe/parse/traditional", test_parse_traditional);
    g_test_add_func("/recipe/parse/git", test_parse_git);
    g_test_add_func("/recipe/parse/bad/recipe/params", test_parse_bad_recipe_params);
    g_test_add_func("/recipe/parse/bad/task/params", test_parse_bad_task_params);
    g_test_add_func("/recipe/parse/bad/recipe/roles", test_parse_bad_recipe_roles);
    g_test_add_func("/recipe/parse/bad/task/roles", test_parse_bad_task_roles);
    return g_test_run();
}
