
#include <glib.h>
#include "role.h"

Role *restraint_role_new(void) {
    return g_slice_new0(Role);
}

void restraint_role_free(Role *role) {
    if (role->value != NULL)
        g_free(role->value);
    if (role->systems != NULL)
        g_free(role->systems);
    g_slice_free(Role, role);
}

