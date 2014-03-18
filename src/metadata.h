
#define RESTRAINT_METADATA_PARSE_ERROR restraint_metadata_parse_error_quark()
#define VAR_LIB_PATH "/var/lib/restraint"
GQuark restraint_metadata_parse_error_quark(void);
typedef enum {
    RESTRAINT_METADATA_PARSE_ERROR_BAD_SYNTAX, /* parse errors from metadata*/
    RESTRAINT_METADATA_MISSING_FILE, /* Missing file*/
    RESTRAINT_METADATA_OPEN, /* Unable to open file*/
} RestraintMetadataParseError;

gboolean restraint_metadata_parse(Task *task, GError **error);
gboolean restraint_generate_testinfo(AppData *app_data, GError **error);
gboolean restraint_is_rhts_compat (Task *task);
gboolean restraint_no_testinfo (Task *task);
void restraint_parse_run_metadata (Task *task, GError **error);
void restraint_set_run_metadata (Task *task, gchar *key, GError **error, GType type, ...);
void restraint_set_running_config (gchar *key, gchar *value, GError **error);
gchar * restraint_get_running_config (gchar *key, GError **error);
