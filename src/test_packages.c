
#include <glib.h>

#include "packages.h"

static void test_yum_install_success(void) {
    GError *error = NULL;
    gboolean succeeded = restraint_install_package("succeed", &error);
    g_assert(succeeded);
    g_assert_no_error(error);
}

static void test_yum_install_failure(void) {
    GError *error = NULL;
    // The package name "fail" is hardcoded in dummy yum to fail
    gboolean succeeded = restraint_install_package("fail", &error);
    g_assert(!succeeded);
    g_assert_error(error, G_SPAWN_EXIT_ERROR, 1);
    g_error_free(error);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/packages/yum_install/success", test_yum_install_success);
    g_test_add_func("/packages/yum_install/failure", test_yum_install_failure);
    return g_test_run();
}
