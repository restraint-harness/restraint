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
    g_clear_error (&error);
}

static void
dependency_rpm(DependencyData *dependency_data)
{
    GError *error = NULL;
    if (dependency_data->dependencies) {
        gchar *package_name = dependency_data->dependencies->data;
        // FIXME: use a generic shell wrapper to abstract away
        // different system install comamnds, yum, apt-get, up2date, etc..
        gchar *command;
        if (g_str_has_prefix (package_name, "-") == TRUE) {
            command = g_strdup_printf ("yum -y remove %s", &package_name[1]);
        } else {
            command = g_strdup_printf ("yum -y install %s", package_name);
        }

        process_run ((const gchar *)command,
                     NULL,
                     NULL,
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
    g_clear_error (&error);
}

static void
restraint_fetch_repodeps(DependencyData *dependency_data)
{
    if (dependency_data->repodeps != NULL) {
        RepoDepData *rd_data = g_slice_new0(RepoDepData);
        uint32_t pplen = dependency_data->path_prefix_len;
        rd_data->dependency_data = dependency_data;
        rd_data->url = soup_uri_copy(dependency_data->fetch_url);
        if (*dependency_data->main_task_name == '/' &&
            *(char*)dependency_data->repodeps->data != '/') {
          pplen -= 1;
        }
        soup_uri_set_fragment(rd_data->url,
                              (char *)dependency_data->repodeps->data +
                              pplen);
        rd_data->path = g_build_filename(TASK_LOCATION,
                                          soup_uri_get_host(rd_data->url),
                                          soup_uri_get_path(rd_data->url),
                                          soup_uri_get_fragment(rd_data->url),
                                          NULL);
        if (g_strcmp0(soup_uri_get_scheme(rd_data->url), "git") == 0) {
            restraint_fetch_git(rd_data->url, rd_data->path, NULL,
                                fetch_repodeps_finish_callback, rd_data);
        } else {
            restraint_fetch_http(rd_data->url, rd_data->path, NULL,
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

static uint32_t
get_path_prefix_len(Task *task)
{
  uint32_t result = 0;
  const char *fragment = soup_uri_get_fragment(task->fetch.url);
  if (fragment != NULL) {
      gchar *strend = g_strrstr(task->name, fragment);
      if (strend != NULL) {
        result = strend - task->name;
      }
  }
  return result;
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
    dependency_data->fetch_url = task->fetch.url;
    dependency_data->path_prefix_len = get_path_prefix_len(task);
    dependency_data->main_task_name = task->name;
    dependency_data->ignore_failed_install = task->rhts_compat;
    dependency_data->io_callback = io_callback;
    dependency_data->finish_cb = finish_cb;
    dependency_data->cancellable = cancellable;
    dependency_data->state = DEPENDENCY_REPO;

    dependency_handler (dependency_data);
}
