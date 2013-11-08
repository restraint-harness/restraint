
#define RESTRAINT_METADATA_PARSE_ERROR restraint_metadata_parse_error_quark()
GQuark restraint_metadata_parse_error_quark(void);
typedef enum {
    RESTRAINT_METADATA_PARSE_ERROR_BAD_SYNTAX, /* parse errors from metadata*/
    RESTRAINT_METADATA_MISSING_FILE, /* Missing file*/
    RESTRAINT_METADATA_OPEN, /* Unable to open file*/
} RestraintMetadataParseError;

gboolean restraint_metadata_update(Task *task, GError **error);
void restraint_parse_run_metadata (Task *task, GError **error);
void restraint_set_run_metadata (Task *task, gchar *key, GError **error, GType type, ...);
