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
#include <string.h>

#include "env.h"
#include "errors.h"
#include "recipe.h"
#include "task.h"
#include "role.h"
#include "param.h"

static void get_env_rmembers(gchar *env, gchar **rmembers)
{
  if (env != NULL) {
    gchar **envval = g_strsplit(env, "=", 2);
    if (g_strcmp0(envval[0], "RECIPE_MEMBERS") == 0) {
      *rmembers = g_strdup(envval[1]);
    }
    g_strfreev(envval);
  }
}

static void test_task_env_role_members_standalone(void)
{
  Task *task = g_slice_new0(Task);
  Param rmem = {"RECIPE_MEMBERS", "otherhost localhost"};
  gchar *rmembers = NULL;

  task->rhts_compat = FALSE;
  task->recipe = g_slice_new0(Recipe);
  task->params = g_list_append(task->params, &rmem);

  build_env("http://localhost", FALSE, task);
  g_ptr_array_foreach(task->env, (GFunc)get_env_rmembers, &rmembers);

  g_assert_cmpstr(rmembers, ==, "otherhost localhost");

  g_slice_free(Recipe, task->recipe);

  if (task->env != NULL) {
    g_ptr_array_free(task->env, TRUE);
  }
  g_list_free(task->params);
  g_slice_free(Task, task);

  if (rmembers != NULL) {
    g_free(rmembers);
  }
}

static void test_task_env_role_members_beaker(void)
{
  Task *task = g_slice_new0(Task);
  GList *roles = NULL;
  gchar *rmembers = NULL;

  Role srv = { "SERVERS", "localhost" };
  Role clt = { "CLIENTS", "otherhost" };
  Role dupe = { "CLIENTS", "localhost" };

  roles = g_list_append(roles, &srv);
  roles = g_list_append(roles, &clt);
  roles = g_list_append(roles, &dupe);

  task->rhts_compat = FALSE;
  task->recipe = g_slice_new0(Recipe);
  task->roles = roles;

  build_env("http://localhost", FALSE, task);
  g_ptr_array_foreach(task->env, (GFunc)get_env_rmembers, &rmembers);

  g_assert_cmpstr(rmembers, ==, "otherhost localhost");

  g_list_free(roles);

  g_slice_free(Recipe, task->recipe);

  if (task->env != NULL) {
    g_ptr_array_free(task->env, TRUE);
  }
  g_slice_free(Task, task);

  if (rmembers != NULL) {
    g_free(rmembers);
  }
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/task/env/role_members/standalone",
      test_task_env_role_members_standalone);
  g_test_add_func("/task/env/role_members/beaker",
      test_task_env_role_members_beaker);
  return g_test_run();
}
