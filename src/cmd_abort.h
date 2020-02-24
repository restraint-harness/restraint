#ifndef RESTRAINT_CMD_ABORT
#define RESTRAINT_CMD_ABORT

#include <glib.h>
#include "cmd_utils.h"

typedef struct {
    ServerData s;
    gchar *type;
} AbortAppData;


gboolean upload_abort(AbortAppData *app_data, GError **error);
gboolean parse_abort_arguments(AbortAppData *app_data, int argc, char *argv[], GError **error);

#endif
