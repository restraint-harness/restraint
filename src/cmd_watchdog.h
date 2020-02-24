#ifndef RESTRAINT_CMD_WD
#define RESTRAINT_CMD_WD

#include <glib.h>
#include "cmd_utils.h"

typedef struct {
    ServerData s;
    guint64 seconds;
} WatchdogAppData;

gboolean upload_watchdog(WatchdogAppData *app_data, GError **error);
gboolean parse_watchdog_arguments(WatchdogAppData *app_data, int argc, char *argv[],
                                  GError **error);

#endif
