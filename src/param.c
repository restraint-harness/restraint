
#include <glib.h>
#include "param.h"

Param *restraint_param_new(void) {
    return g_slice_new0(Param);
}

void restraint_param_free(Param *param) {
    if (param->name != NULL)
        g_free(param->name);
    if (param->value != NULL)
        g_free(param->value);
    g_slice_free(Param, param);
}

