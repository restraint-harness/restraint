#ifndef RESTRAINT_CMD_LOG
#define RESTRAINT_CMD_LOG

#include <glib.h>
#include "cmd_utils.h"

typedef struct {
    ServerData s;
    gchar *filename;
    gchar *deprecated1;
    gchar *deprecated2;
} LogAppData;

gboolean upload_log (LogAppData *app_data, GError **error);
gboolean parse_log_arguments(LogAppData *app_data, int argc, char *argv[],
                             GError **error);

#endif
