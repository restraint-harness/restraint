
#include "packages.h"

/* Ideas for the future:
 * abstract away yum specifics
 * call yum code more directly, or use PackageKit, or re-use code from PackageKit
 */

gboolean restraint_install_package(const gchar *package_name, GError **error) {
    g_return_val_if_fail(package_name != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    const gchar *argv[] = { "yum", "-y", "install", package_name, NULL };
    gchar *yum_stdout = NULL;
    gchar *yum_stderr = NULL;
    gint yum_exit_status = -1;
    GError *tmp_error = NULL;
    gboolean spawn_succeeded = g_spawn_sync(
            /* cwd */ "/",
            (gchar **) argv,
            /* envp */ NULL,
            G_SPAWN_SEARCH_PATH,
            /* child setup closure */ NULL, NULL,
            &yum_stdout,
            &yum_stderr,
            &yum_exit_status,
            &tmp_error);
    if (!spawn_succeeded) {
        g_propagate_prefixed_error(error, tmp_error,
                "While installing package %s: ", package_name);
        return FALSE;
    }
    if (!g_spawn_check_exit_status(yum_exit_status, &tmp_error)) {
        g_propagate_prefixed_error(error, tmp_error,
                "While installing package %s: ", package_name);
        // XXX include yum's stderr in the error message
        g_free(yum_stdout);
        g_free(yum_stderr);
        return FALSE;
    }
    g_free(yum_stdout);
    g_free(yum_stderr);
    return TRUE;
}
