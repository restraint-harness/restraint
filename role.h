
#ifndef _RESTRAINT_ROLE_H
#define _RESTRAINT_ROLE_H

#include <glib.h>

typedef struct {
    gchar *value;
    gchar *systems;
} Role;

Role *restraint_role_new(void);
void restraint_role_free(Role *role);
#endif
