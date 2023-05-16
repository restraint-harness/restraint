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

#ifndef _RESTRAINT_DEPENDENCY_H
#define _RESTRAINT_DEPENDENCY_H

#include <glib.h>
#include <stdint.h>
#include "recipe.h"
#include "task.h"
#include "fetch.h"

typedef void (*DependencyCallback)   (gpointer user_data, GError *error);

typedef enum {
    DEPENDENCY_REPO,
    DEPENDENCY_RPM,
    DEPENDENCY_SINGLE_RPM,
    DEPENDENCY_SOFT_RPM,
    DEPENDENCY_DONE
} DependencyState;

typedef struct {
    GSList *dependencies;
    GSList *softdependencies;
    GSList *repodeps;
    GSList *processed_deps;
    SoupURI *fetch_url;
    gboolean keepchanges;
    const gchar *main_task_name;
    const gchar *base_path;
    gboolean ignore_failed_install;
    GIOFunc io_callback;
    ArchiveEntryCallback archive_entry_callback;
    DependencyCallback finish_cb;
    GCancellable *cancellable;
    DependencyState state;
    gpointer user_data;
    char *osmajor;
    GString *install_rpms;
    GString *remove_rpms;
    gboolean ssl_verify;
    gboolean abort_recipe_on_fail;
} DependencyData;

void restraint_install_dependencies (Task *task, GIOFunc io_callback,
                                     ArchiveEntryCallback archive_entry_callback,
                                     DependencyCallback finish_cb,
                                     GCancellable *cancellable,
                                     gpointer user_data);

#endif
