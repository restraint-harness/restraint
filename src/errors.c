
#include <glib.h>

GQuark restraint_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-parse-error-quark");
}

