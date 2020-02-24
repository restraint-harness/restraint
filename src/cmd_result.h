#ifndef RESTRAINT_CMD_RESULT
#define RESTRAINT_CMD_RESULT

#include <glib.h>
#include "cmd_utils.h"

typedef struct {
    ServerData s;
    gchar *filename;
    gchar *outputfile;
    GPtrArray *disable_plugin;

    gchar *result_msg;
    gchar *prefix;

    /* Positional Arguments */
    gchar *test_name;
    gchar *test_result;
    gchar *score;

    gboolean no_plugins;
    gboolean rhts_compat;
} AppData;

gboolean parse_arguments(AppData *app_data, int argc, char *argv[]);
void restraint_free_appdata(AppData *app_data);
AppData* restraint_create_appdata();
gboolean upload_results(AppData *app_data);

#endif
