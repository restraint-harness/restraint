
#include <glib.h>

#include "task.h"

static void test_something(void) {
    g_assert(TRUE);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/fetch_task/something", test_something);
    return g_test_run();
}
