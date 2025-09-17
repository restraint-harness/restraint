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

#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <archive.h>

#include "dependency.h"
#include "errors.h"

typedef struct {
    GError *error;
    GMainLoop *loop;
    GString *output;
} RunData;

gboolean
dependency_io_cb (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    GError *error = NULL;

    gchar buf[131072];
    gsize bytes_read;

    if (condition & (G_IO_IN )) {
        switch (g_io_channel_read_chars(io, buf, 131072, &bytes_read, &error)) {
          case G_IO_STATUS_NORMAL:
            g_string_append_len (run_data->output, buf, bytes_read);
            return TRUE;

          case G_IO_STATUS_ERROR:
             g_clear_error (&error);
             return FALSE;

          case G_IO_STATUS_EOF:
             g_string_append_len (run_data->output, "finished!\n", 10);
             return FALSE;

          case G_IO_STATUS_AGAIN:
             g_string_append_len (run_data->output, "Not ready.. try again.\n", 24);
             return TRUE;

          default:
             g_return_val_if_reached(FALSE);
             break;
        }
    }

    if (condition & G_IO_HUP){
        return FALSE;
    }
    return FALSE;
}

void
dependency_finish_cb (gpointer user_data, GError *error)
{
    RunData *run_data = (RunData *) user_data;
    if (error)
        g_propagate_error(&run_data->error, error);
    if (run_data->loop) {
        g_main_loop_quit (run_data->loop);
        g_main_loop_unref (run_data->loop);
    }
}

static void test_soft_dependencies_success (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    GSList *softdependencies = NULL;
    softdependencies = g_slist_prepend (dependencies, "PackageA");
    softdependencies = g_slist_prepend (softdependencies, "Packagefail");
    softdependencies = g_slist_prepend (softdependencies, "PackageC");
    gchar *expected = "use_pty:FALSE rstrnt-package install PackageC\n"
                      "dummy yum: installing PackageC\n"
                      "use_pty:FALSE rstrnt-package install Packagefail\n"
                      "dummy yum: fail\ndummy rpm: fail\n"
                      "use_pty:FALSE rstrnt-package install PackageA\n"
                      "dummy yum: installing PackageA\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = dependencies;
    task->metadata->softdependencies = softdependencies;
    task->metadata->repodeps = NULL;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fetch_git";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (softdependencies);

    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_dependencies_success (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "PackageB");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "use_pty:FALSE rstrnt-package install PackageC PackageB PackageA\n"
                      "dummy yum: installing PackageC PackageB PackageA\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = dependencies;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = NULL;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fetch_git";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);

    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_dependencies_fail (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "Packagefail");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "use_pty:FALSE rstrnt-package install PackageC Packagefail PackageA\n"
                      "dummy yum: fail\ndummy rpm: fail\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = dependencies;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = NULL;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fetch_git";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_ERROR, RESTRAINT_TASK_RUNNER_RC_ERROR);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);

    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_dependencies_ignore_fail (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "Packagefail");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "use_pty:FALSE rstrnt-package install PackageC Packagefail PackageA\n"
                      "dummy yum: fail\ndummy rpm: fail\n"
                      "use_pty:FALSE rstrnt-package install PackageC\n"
                      "dummy yum: installing PackageC\n"
                      "use_pty:FALSE rstrnt-package install Packagefail\n"
                      "dummy yum: fail\ndummy rpm: fail\n"
                      "use_pty:FALSE rstrnt-package install PackageA\n"
                      "dummy yum: installing PackageA\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = dependencies;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = NULL;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fetch_git");
    task->rhts_compat = TRUE;
    task->name = "restraint/sanity/fetch_git";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);

    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_git_repodeps_success (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/common");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);

    gchar *fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/common");
    GFile *base = g_file_new_for_path(fullpath);

    GFile *file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_git_repodeps_fail (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/nonexistant");

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_FATAL);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_http_repodeps_success (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/common");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("http://localhost:8000/fetch_http.tgz#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_http_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);

    gchar *fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/common");
    GFile *base = g_file_new_for_path(fullpath);

    GFile *file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_http_repodeps_fail (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/nonexistant");

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("http://localhost:8000/fetch_http.tgz#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_repodep_http_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_WARN);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_git_rec_repodeps_success (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/fetch_git");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_rec_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);

    gchar *fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/fetch_git");
    GFile *base = g_file_new_for_path(fullpath);

    GFile *file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/common");
    base = g_file_new_for_path(fullpath);

    file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_git_rec_repodeps_fail (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/rdep_fail");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("git://localhost/repo1?master#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_rec_repodep_git_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_FATAL);
    g_clear_error (&run_data->error);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_http_rec_repodeps_success (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/fetch_git");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("http://localhost:8000/fetch_http.tgz#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_rec_repodep_http_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);

    gchar *fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/fetch_git");
    GFile *base = g_file_new_for_path(fullpath);

    GFile *file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    fullpath = g_strdup_printf("%s/%s/%s/%s", task->recipe->base_path,
            task->fetch.url->host,
            task->fetch.url->path,
            "restraint/sanity/common");
    base = g_file_new_for_path(fullpath);

    file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);
    g_free(fullpath);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

static void test_http_rec_repodeps_fail (void)
{
    RunData *run_data;
    GSList *repodeps = NULL;
    repodeps = g_slist_prepend(repodeps, "restraint/sanity/rdep_fail");

    run_data = g_slice_new0 (RunData);
    run_data->output = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    Task *task = g_slice_new0(Task);
    task->fetch_method = TASK_FETCH_UNPACK;
    task->metadata = g_slice_new(MetaData);
    task->metadata->dependencies = NULL;
    task->metadata->softdependencies = NULL;
    task->metadata->repodeps = repodeps;
    task->fetch.url = soup_uri_new("http://localhost:8000/fetch_http.tgz#restraint/sanity/fake");
    task->rhts_compat = FALSE;
    task->name = "restraint/sanity/fake";
    task->recipe = g_slice_new0(Recipe);
    task->recipe->base_path = g_dir_make_tmp("test_rec_repodep_http_XXXXXX", NULL);

    restraint_install_dependencies (task,
                                    dependency_io_cb,
                                    NULL,
                                    dependency_finish_cb,
                                    NULL,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_WARN);
    g_clear_error (&run_data->error);

    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (repodeps);
    soup_uri_free(task->fetch.url);
    g_remove (task->recipe->base_path);
    g_free (task->recipe->base_path);
    g_slice_free(Recipe, task->recipe);
    g_slice_free(MetaData, task->metadata);
    g_slice_free(Task, task);
}

int main(int argc, char *argv[]) {
    putenv("RSTRNT_PKG_CMD=fakeyum");
    putenv("RSTRNT_PKG_MANUAL_INSTALL=fakerpm --nodigest -ivh");
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/dependencies/success", test_dependencies_success);
    g_test_add_func("/dependencies/failure", test_dependencies_fail);
    g_test_add_func("/dependencies/ignore_failure", test_dependencies_ignore_fail);
    g_test_add_func("/softdependencies/success", test_soft_dependencies_success);
    g_test_add_func("/repodeps/git/success", test_git_repodeps_success);
    g_test_add_func("/repodeps/git/fail", test_git_repodeps_fail);
    g_test_add_func("/repodeps/http/success", test_http_repodeps_success);
    g_test_add_func("/repodeps/http/fail", test_http_repodeps_fail);
    g_test_add_func("/repodeps/recursive/git/success", test_git_rec_repodeps_success);
    g_test_add_func("/repodeps/recursive/git/fail", test_git_rec_repodeps_fail);
    g_test_add_func("/repodeps/recursive/http/success", test_http_rec_repodeps_success);
    g_test_add_func("/repodeps/recursive/http/fail", test_http_rec_repodeps_fail);
    return g_test_run();
}
