
#include <glib.h>

#include "fetch_task.h"

static void test_something(void) {
    restraint_fetch_task();
    g_assert(TRUE);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/fetch_task/something", test_something);
    return g_test_run();
}
