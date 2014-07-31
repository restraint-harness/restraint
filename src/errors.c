
#include <glib.h>

GQuark restraint_error_quark(void) {
    return g_quark_from_static_string("restraint-error-quark");
}

