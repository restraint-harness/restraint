
#ifndef _RESTRAINT_PARAM_H
#define _RESTRAINT_PARAM_H

#include <glib.h>

typedef struct {
    gchar *name;
    gchar *value;
} Param;

Param *restraint_param_new(void);
void restraint_param_free(Param *param);
#endif
