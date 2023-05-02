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

#include <string.h>
#include <libsoup/soup.h>

#include "dependency.h"
#include "errors.h"
#include "process.h"
#include "fetch_git.h"
#include "fetch_uri.h"

typedef struct {
    SoupURI *url;
    gchar *path;
    DependencyData *dependency_data;
} RepoDepData;

typedef struct {
    char *path;
    MetaData *tmp_meta;
    DependencyData *dependency_data;
} MetadataFetchInfo;

static void dependency_handler (gpointer user_data);
static void restraint_fetch_repodeps(DependencyData *dependency_data);
static void dependency_batch_rpms(DependencyData *dependency_data);

static void
mtfi_archive_entry_callback (const gchar *entry, gpointer user_data)
{
    MetadataFetchInfo *mtfi = (MetadataFetchInfo*)user_data;
    DependencyData *dependency_data = mtfi->dependency_data;
    if (dependency_data->archive_entry_callback != NULL) {
        return dependency_data->archive_entry_callback (entry, dependency_data->user_data);
    }
}

static void
repo_dep_data_archive_callback (const gchar *entry, gpointer user_data)
{
    RepoDepData *rd_data = (RepoDepData*)user_data;
    DependencyData *dependency_data = rd_data->dependency_data;
    if (dependency_data->archive_entry_callback != NULL) {
        return dependency_data->archive_entry_callback (entry, dependency_data->user_data);
    }
}

static gboolean
dependency_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    DependencyData *dependency_data = (DependencyData *) user_data;
    return dependency_data->io_callback (io, condition, dependency_data->user_data);
}

static gboolean
mtfi_io_callback (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    MetadataFetchInfo *mtfi_data = (MetadataFetchInfo *) user_data;
    return dependency_io_callback (io, condition, mtfi_data->dependency_data);
}

static gboolean dependency_process_errors(DependencyData *dependency_data,
                                          GError *error, gint pid_result)
{
    if (error) {
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb (dependency_data->user_data, error);
        }

        if (dependency_data->remove_rpms != NULL) {
            g_string_free(dependency_data->remove_rpms, TRUE);
            dependency_data->remove_rpms = NULL;
        }
        if (dependency_data->install_rpms != NULL) {
            g_string_free(dependency_data->install_rpms, TRUE);
            dependency_data->install_rpms = NULL;
        }
        g_slist_free_full(dependency_data->processed_deps, g_free);
        g_slice_free (DependencyData, dependency_data);
        return FALSE;
    } else if (dependency_data->ignore_failed_install != TRUE && pid_result != 0) {
        // If running in rhts_compat mode we don't check whether a packge installed
        // or not.
        // failed to install or remove a package, report fail and abort task
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_TASK_RUNNER_RC_ERROR,
                     "Command returned non-zero %i", pid_result);
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb (dependency_data->user_data, error);
        }

        if (dependency_data->remove_rpms != NULL) {
            g_string_free(dependency_data->remove_rpms, TRUE);
            dependency_data->remove_rpms = NULL;
        }
        if (dependency_data->install_rpms != NULL) {
            g_string_free(dependency_data->install_rpms, TRUE);
            dependency_data->install_rpms = NULL;
        }
        g_slist_free_full(dependency_data->processed_deps, g_free);
        g_slice_free (DependencyData, dependency_data);
        return FALSE;
    } else {
        return TRUE;
    }
}

static void
dependency_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    DependencyData *dependency_data = (DependencyData *) user_data;

    g_cancellable_set_error_if_cancelled (dependency_data->cancellable, &error);

    if (dependency_process_errors(dependency_data, error, pid_result)) {
        dependency_data->dependencies = dependency_data->dependencies->next;
        dependency_handler (dependency_data);
    }
}

static void dependency_batch_remove_cb(gint pid_result,
                                       gboolean localwatchdog,
                                       gpointer user_data, GError *error)
{
    DependencyData *dependency_data = (DependencyData *) user_data;

    g_cancellable_set_error_if_cancelled (dependency_data->cancellable, &error);

    if (dependency_process_errors(dependency_data, error, pid_result)) {
        if (dependency_data->ignore_failed_install == TRUE &&
                !error && pid_result != 0) {
            // Fall back to one-by-one installation mode

            if (dependency_data->remove_rpms != NULL) {
                g_string_free(dependency_data->remove_rpms, TRUE);
                dependency_data->remove_rpms = NULL;
            }
            if (dependency_data->install_rpms != NULL) {
                g_string_free(dependency_data->install_rpms, TRUE);
                dependency_data->install_rpms = NULL;
            }
            dependency_data->state = DEPENDENCY_SINGLE_RPM;
            dependency_handler(dependency_data);
        } else {
            g_string_free(dependency_data->remove_rpms, TRUE);
            dependency_data->remove_rpms = NULL;
            dependency_batch_rpms(dependency_data);
        }
    }
}

static void dependency_batch_install_cb(gint pid_result,
                                        gboolean localwatchdog,
                                        gpointer user_data, GError *error)
{
    DependencyData *dependency_data = (DependencyData *) user_data;

    g_cancellable_set_error_if_cancelled (dependency_data->cancellable, &error);

    if (dependency_process_errors(dependency_data, error, pid_result)) {
        if (dependency_data->ignore_failed_install == TRUE &&
                !error && pid_result != 0) {
            // Fall back to one-by-one installation mode

            if (dependency_data->remove_rpms != NULL) {
                g_string_free(dependency_data->remove_rpms, TRUE);
                dependency_data->remove_rpms = NULL;
            }
            if (dependency_data->install_rpms != NULL) {
                g_string_free(dependency_data->install_rpms, TRUE);
                dependency_data->install_rpms = NULL;
            }
            dependency_data->state = DEPENDENCY_SINGLE_RPM;
            dependency_handler(dependency_data);
        } else {
            g_string_free(dependency_data->install_rpms, TRUE);
            dependency_data->install_rpms = NULL;

            dependency_data->dependencies = NULL;

            dependency_data->state = DEPENDENCY_SOFT_RPM;
            dependency_handler(dependency_data);
        }
    }
}

static void
dependency_batch_rpms(DependencyData *dependency_data)
{
    if (dependency_data->remove_rpms != NULL) {
        if (dependency_data->remove_rpms->len > 0) {
            gchar *command = g_strdup_printf("rstrnt-package remove%s",
                                             dependency_data->remove_rpms->str);

            process_run ((const gchar *)command,
                         NULL,
                         NULL,
                         FALSE,
                         0,
                         NULL,
                         dependency_io_callback,
                         dependency_batch_remove_cb,
                         NULL,
                         0,
                         FALSE,
                         dependency_data->cancellable,
                         dependency_data);
            g_free (command);
        } else {
            g_string_free(dependency_data->remove_rpms, TRUE);
            dependency_data->remove_rpms = NULL;
            dependency_batch_rpms(dependency_data);
        }
    } else if (dependency_data->install_rpms != NULL) {
        if (dependency_data->install_rpms->len > 0) {
            gchar *command = g_strdup_printf("rstrnt-package install%s",
                                             dependency_data->install_rpms->str);

            process_run ((const gchar *)command,
                         NULL,
                         NULL,
                         FALSE,
                         0,
                         NULL,
                         dependency_io_callback,
                         dependency_batch_install_cb,
                         NULL,
                         0,
                         FALSE,
                         dependency_data->cancellable,
                         dependency_data);
            g_free (command);
        } else {
            g_string_free(dependency_data->install_rpms, TRUE);
            dependency_data->install_rpms = NULL;
            dependency_batch_rpms(dependency_data);
        }
    } else {
        dependency_data->state = DEPENDENCY_SOFT_RPM;
        dependency_handler(dependency_data);
    }
}

static void
softdependency_callback (gint pid_result, gboolean localwatchdog, gpointer user_data, GError *error)
{
    DependencyData *dependency_data = (DependencyData *) user_data;

    g_cancellable_set_error_if_cancelled (dependency_data->cancellable, &error);

    if (error) {
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb (dependency_data->user_data, error);
        }

        g_slist_free_full(dependency_data->processed_deps, g_free);
        g_slice_free (DependencyData, dependency_data);
    } else {
        dependency_data->softdependencies = dependency_data->softdependencies->next;
        dependency_handler (dependency_data);
    }
}

static void
dependency_rpm(DependencyData *dependency_data)
{
    GError *error = NULL;

    if (dependency_data->dependencies) {
        dependency_data->install_rpms = g_string_new(NULL);
        dependency_data->remove_rpms = g_string_new(NULL);

        for (GSList *l = dependency_data->dependencies; l; l = g_slist_next(l)) {
            gchar *package_name = l->data;
            if (g_str_has_prefix (package_name, "-") == TRUE) {
                g_string_append_printf(dependency_data->remove_rpms,
                                       " %s", package_name + 1);
            } else {
                g_string_append_printf(dependency_data->install_rpms,
                                       " %s", package_name);
            }
        }

        dependency_batch_rpms(dependency_data);
    } else {
        // no more packages to install/remove
        // move on to next stage.
        dependency_data->state = DEPENDENCY_SOFT_RPM;
        dependency_handler(dependency_data);
    }
    g_clear_error (&error);

}

static void
dependency_soft_rpm(DependencyData *dependency_data)
{
    GError *error = NULL;

    if (dependency_data->softdependencies) {
        gchar *package_name = dependency_data->softdependencies->data;
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
                     NULL,
                     dependency_io_callback,
                     softdependency_callback,
                     NULL,
                     0,
                     FALSE,
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
dependency_single_rpm(DependencyData *dependency_data)
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
                     NULL,
                     dependency_io_callback,
                     dependency_callback,
                     NULL,
                     0,
                     FALSE,
                     dependency_data->cancellable,
                     dependency_data);
        g_free (command);
    } else {
        // no more packages to install/remove
        // move on to next stage.
        dependency_data->state = DEPENDENCY_SOFT_RPM;
        dependency_handler(dependency_data);
    }
    g_clear_error (&error);

}

static void ldep_finish_cb(gpointer user_data, GError *error)
{
    MetadataFetchInfo *mtfi = (MetadataFetchInfo*)user_data;
    DependencyData *dependency_data = mtfi->dependency_data;

    if (error) {
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb (dependency_data->user_data, error);
        }
        g_slist_free_full(dependency_data->processed_deps, g_free);
        g_slice_free (DependencyData, dependency_data);
    } else {
        dependency_data->repodeps = g_slist_next(dependency_data->repodeps);
        restraint_fetch_repodeps(dependency_data);
    }

    g_free(mtfi->path);
    restraint_metadata_free(mtfi->tmp_meta);
    g_slice_free(MetadataFetchInfo, mtfi);
}

static gboolean ldep_io_cb(GIOChannel *io, GIOCondition condition,
                           gpointer user_data)
{
    MetadataFetchInfo *mtfi = (MetadataFetchInfo*)user_data;
    DependencyData *dependency_data = mtfi->dependency_data;
    return dependency_data->io_callback(io, condition,
                                        dependency_data->user_data);
}

static void dep_mtdata_finish_cb(gpointer user_data, GError *error)
{
    MetadataFetchInfo *mtfi = (MetadataFetchInfo*)user_data;
    DependencyData *dependency_data = mtfi->dependency_data;

    if (error) {
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb(dependency_data->user_data, error);
        }
        g_free(mtfi->path);
        g_slice_free(MetadataFetchInfo, mtfi);
    } else if (mtfi->tmp_meta != NULL) {
        DependencyData *newdd = g_slice_dup(DependencyData, dependency_data);
        newdd->dependencies = mtfi->tmp_meta->dependencies;
        newdd->repodeps = mtfi->tmp_meta->repodeps;
        newdd->finish_cb = ldep_finish_cb;
        newdd->io_callback = ldep_io_cb;
        newdd->archive_entry_callback = mtfi_archive_entry_callback;
        newdd->user_data = mtfi;
        newdd->processed_deps = g_slist_copy_deep(
                                    dependency_data->processed_deps,
                                    (GCopyFunc)g_strdup, NULL);
        restraint_fetch_repodeps(newdd);
    } else {
        g_warning("No metadata for dependency '%s'\n", mtfi->path);
        g_free(mtfi->path);
        g_slice_free(MetadataFetchInfo, mtfi);
        dependency_data->repodeps = g_slist_next(dependency_data->repodeps);
        restraint_fetch_repodeps(dependency_data);
    }
}

static void
fetch_repodeps_finish_callback(GError *error, guint32 match_cnt,
                               guint32 nonmatch_cnt, gpointer user_data)
{
    RepoDepData *rd_data = (RepoDepData*)user_data;
    DependencyData *dependency_data = rd_data->dependency_data;

    GSList *pdep = g_slist_find_custom(dependency_data->processed_deps, rd_data->path, (GCompareFunc)g_strcmp0);
    if (error) {
        if (dependency_data->finish_cb) {
            dependency_data->finish_cb (dependency_data->user_data, error);
        }
        g_slist_free_full(dependency_data->processed_deps, g_free);
        g_slice_free (DependencyData, dependency_data);
    } else if (pdep == NULL) {
        MetadataFetchInfo *mtfi = g_slice_new0(MetadataFetchInfo);
        dependency_data->processed_deps = g_slist_prepend(
                                            dependency_data->processed_deps,
                                            g_strdup(rd_data->path));
        mtfi->path = g_strdup(rd_data->path);
        mtfi->dependency_data = dependency_data;
        restraint_get_metadata(mtfi->path, dependency_data->osmajor,
                               &mtfi->tmp_meta,
                               dependency_data->cancellable,
                               dep_mtdata_finish_cb, mtfi_io_callback, mtfi);
    } else {
        dependency_data->repodeps = g_slist_next(dependency_data->repodeps);
        restraint_fetch_repodeps(dependency_data);
    }
    soup_uri_free(rd_data->url);
    g_free(rd_data->path);
    g_slice_free(RepoDepData, rd_data);
}

static void
restraint_fetch_repodeps(DependencyData *dependency_data)
{
    if (dependency_data->repodeps != NULL) {
        RepoDepData *rd_data = g_slice_new0(RepoDepData);
        rd_data->dependency_data = dependency_data;
        rd_data->url = soup_uri_copy(dependency_data->fetch_url);
        g_free(rd_data->url->fragment);
        rd_data->url->fragment = g_strdup((char *)dependency_data->repodeps->data);
        rd_data->path = g_build_filename(dependency_data->base_path,
                                         rd_data->url->host,
                                         rd_data->url->path,
                                         rd_data->url->fragment,
                                         NULL);
        if (g_strcmp0(rd_data->url->scheme, "git") == 0) {
            restraint_fetch_git(rd_data->url, rd_data->path,
                                dependency_data->keepchanges, repo_dep_data_archive_callback,
                                fetch_repodeps_finish_callback, rd_data);
        } else {
            restraint_fetch_uri(rd_data->url, rd_data->path,
                                 dependency_data->keepchanges, dependency_data->ssl_verify, dependency_data->abort_recipeset_on_fail, repo_dep_data_archive_callback,
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
        case DEPENDENCY_SINGLE_RPM:
            dependency_single_rpm(dependency_data);
            break;
        case DEPENDENCY_SOFT_RPM:
            dependency_soft_rpm(dependency_data);
            break;
        case DEPENDENCY_DONE:
            if (dependency_data->finish_cb) {
                dependency_data->finish_cb(dependency_data->user_data, NULL);
            }
            g_slist_free_full(dependency_data->processed_deps, g_free);
            g_slice_free (DependencyData, dependency_data);
            break;
        default:
            break;
    }
}

void
restraint_install_dependencies (Task *task,
                                GIOFunc io_callback,
                                ArchiveEntryCallback archive_entry_callback,
                                DependencyCallback finish_cb,
                                GCancellable *cancellable,
                                gpointer user_data)
{
    DependencyData *dependency_data;
    dependency_data = g_slice_new0 (DependencyData);
    dependency_data->user_data = user_data;
    dependency_data->dependencies = task->metadata->dependencies;
    dependency_data->softdependencies = task->metadata->softdependencies;
    dependency_data->repodeps = task->metadata->repodeps;
    dependency_data->main_task_name = task->name;
    dependency_data->base_path = task->recipe->base_path;
    dependency_data->ignore_failed_install = task->rhts_compat;
    dependency_data->keepchanges = task->keepchanges;
    dependency_data->io_callback = io_callback;
    dependency_data->archive_entry_callback = archive_entry_callback;
    dependency_data->finish_cb = finish_cb;
    dependency_data->cancellable = cancellable;
    dependency_data->osmajor = task->recipe->osmajor;
    dependency_data->ssl_verify = task->ssl_verify;
    dependency_data->abort_recipeset_on_fail = task->abort_recipeset_on_fail;
    
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
