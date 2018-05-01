#ifndef RESTRAINT_CMD_RESULT
#define RESTRAINT_CMD_RESULT

#include <glib.h>

typedef struct {
    gchar *filename;
    gchar *outputfile;
    GPtrArray *disable_plugin;

    gchar *server;
    gchar *result_msg;
    gchar *prefix;
    gchar *server_recipe;
    gchar *task_id;

    /* Positional Arguments */
    gchar *test_name;
    gchar *test_result;
    gchar *score;

    gboolean no_plugins;
    gboolean rhts_compat;
} AppData;

/* filename that will activate rhts compatibility mode */
#define RHTS_COMPAT_FILENAME "rhts-report-result"

gboolean parse_arguments(AppData *app_data, int argc, char *argv[]);
void restraint_free_appdata(AppData *app_data);
AppData* restraint_create_appdata();
gboolean upload_results(AppData *app_data);

#endif
