#define RESTRAINT_PARSE_ERROR restraint_parse_error_quark()
GQuark restraint_parse_error_quark (void);

typedef enum {
    RESTRAINT_PARSE_ERROR_BAD_SYNTAX, /* parse errors */
    RESTRAINT_MISSING_FILE, /* Missing file*/
    RESTRAINT_OPEN, /* Unable to open file*/
} RestraintParseError;

