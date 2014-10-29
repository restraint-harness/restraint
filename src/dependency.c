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

static void dependency_handler (gpointer user_data);

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
dependency_handler (gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
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
        dependency_data->finish_cb (dependency_data->user_data, error);
        g_slice_free (DependencyData, dependency_data);
    }
    g_clear_error (&error);
}

void
restraint_install_dependencies (GSList *dependencies,
                                gboolean ignore_failed_install,
                                GIOFunc io_callback,
                                DependencyCallback finish_cb,
                                GCancellable *cancellable,
                                gpointer user_data)
{
    DependencyData *dependency_data;
    dependency_data = g_slice_new0 (DependencyData);
    dependency_data->user_data = user_data;
    dependency_data->dependencies = dependencies;
    dependency_data->ignore_failed_install = ignore_failed_install;
    dependency_data->io_callback = io_callback;
    dependency_data->finish_cb = finish_cb;
    dependency_data->cancellable = cancellable;

    dependency_handler (dependency_data);
}
