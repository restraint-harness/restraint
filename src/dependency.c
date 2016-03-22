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

#include "dependency.h"
#include "errors.h"
#include "process.h"
#include "fetch.h"
#include "fetch_git.h"
#include "fetch_http.h"

typedef struct {
    SoupURI *url;
    gchar *path;
    DependencyData *dependency_data;
} RepoDepData;

static void dependency_handler (gpointer user_data);
static void restraint_fetch_repodeps(DependencyData *dependency_data);

static gboolean
dependency_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
    return dependency_data->io_callback (io, condition, dependency_data->user_data);
}

static void
dependency_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    DependencyData *dependency_data = (DependencyData *) user_data;

    g_cancellable_set_error_if_cancelled (dependency_data->cancellable, &error);

    if (error) {
        dependency_data->finish_cb (dependency_data->user_data, error);
        g_slice_free (DependencyData, dependency_data);
    } else if (dependency_data->ignore_failed_install != TRUE && pid_result != 0) {
        // If running in rhts_compat mode we don't check whether a packge installed
        // or not.
        // failed to install or remove a package, report fail and abort task
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_TASK_RUNNER_RC_ERROR,
                     "Command returned non-zero %i", pid_result);
        dependency_data->finish_cb (dependency_data->user_data, error);
        g_slice_free (DependencyData, dependency_data);
    } else {
        dependency_data->dependencies = dependency_data->dependencies->next;
        dependency_handler (dependency_data);
    }
}

static void
dependency_rpm(DependencyData *dependency_data)
{
    GError *error = NULL;
    if (dependency_data->dependencies) {
        gchar *package_name = dependency_data->dependencies->data;
        gchar *command;
        if (g_str_has_prefix (package_name, "-") == TRUE) {
            command = g_strdup_printf ("rstrnt-package remove %s", &package_name[1]);
        } else {
            command = g_strdup_printf ("rstrnt-package install %s", package_name);
        }

        process_run ((const gchar *)command,
                     NULL,
                     NULL,
                     FALSE,
                     0,
                     dependency_io_callback,
                     dependency_callback,
                     dependency_data->cancellable,
                     dependency_data);
        g_free (command);
    } else {
        // no more packages to install/remove
        // move on to next stage.
        dependency_data->state = DEPENDENCY_DONE;
        dependency_handler(dependency_data);
    }
    g_clear_error (&error);

}

static void
fetch_repodeps_finish_callback(GError *error, gpointer user_data)
{
    RepoDepData *rd_data = (RepoDepData*)user_data;
    DependencyData *dependency_data = rd_data->dependency_data;

    soup_uri_free(rd_data->url);
    g_free(rd_data->path);
    g_slice_free(RepoDepData, rd_data);

    if (error) {
        dependency_data->finish_cb (dependency_data->user_data, error);
        g_slice_free (DependencyData, dependency_data);
    } else {
        dependency_data->repodeps = g_slist_next(dependency_data->repodeps);
        restraint_fetch_repodeps(dependency_data);
    }
}

static void
restraint_fetch_repodeps(DependencyData *dependency_data)
{
    if (dependency_data->repodeps != NULL) {
        RepoDepData *rd_data = g_slice_new0(RepoDepData);
        rd_data->dependency_data = dependency_data;
        rd_data->url = soup_uri_copy(dependency_data->fetch_url);
        soup_uri_set_fragment(rd_data->url,
                              (char *)dependency_data->repodeps->data);
        rd_data->path = g_build_filename(dependency_data->base_path,
                                         soup_uri_get_host(rd_data->url),
                                         soup_uri_get_path(rd_data->url),
                                         soup_uri_get_fragment(rd_data->url),
                                         NULL);
        if (g_strcmp0(soup_uri_get_scheme(rd_data->url), "git") == 0) {
            restraint_fetch_git(rd_data->url, rd_data->path,
                                dependency_data->keepchanges, NULL,
                                fetch_repodeps_finish_callback, rd_data);
        } else {
            restraint_fetch_http(rd_data->url, rd_data->path,
                                 dependency_data->keepchanges, NULL,
                                 fetch_repodeps_finish_callback, rd_data);
        }
    } else {
        dependency_data->state = DEPENDENCY_RPM;
        dependency_handler(dependency_data);
    }
}

static void
dependency_handler (gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
    switch (dependency_data->state) {
        case DEPENDENCY_REPO:
            restraint_fetch_repodeps(dependency_data);
            break;
        case DEPENDENCY_RPM:
            dependency_rpm(dependency_data);
            break;
        case DEPENDENCY_DONE:
            dependency_data->finish_cb(dependency_data->user_data, NULL);
            g_slice_free (DependencyData, dependency_data);
            break;
        default:
            break;
    }
}

void
restraint_install_dependencies (Task *task,
                                GIOFunc io_callback,
                                DependencyCallback finish_cb,
                                GCancellable *cancellable,
                                gpointer user_data)
{
    DependencyData *dependency_data;
    dependency_data = g_slice_new0 (DependencyData);
    dependency_data->user_data = user_data;
    dependency_data->dependencies = task->metadata->dependencies;
    dependency_data->repodeps = task->metadata->repodeps;
    dependency_data->main_task_name = task->name;
    dependency_data->base_path = task->recipe->base_path;
    dependency_data->ignore_failed_install = task->rhts_compat;
    dependency_data->keepchanges = task->keepchanges;
    dependency_data->io_callback = io_callback;
    dependency_data->finish_cb = finish_cb;
    dependency_data->cancellable = cancellable;
    switch (task->fetch_method) {
        case TASK_FETCH_UNPACK:
            dependency_data->fetch_url = task->fetch.url;
            dependency_data->state = DEPENDENCY_REPO;
            break;
        case TASK_FETCH_INSTALL_PACKAGE:
            dependency_data->state = DEPENDENCY_RPM;
            break;
        default:
            dependency_data->state = DEPENDENCY_DONE;
            break;
    }
    dependency_handler (dependency_data);
}
