
#ifndef _RESTRAINT_PACKAGES_H
#define _RESTRAINT_PACKAGES_H

/*
 * Code for manipulating system packages.
 */

#include <glib.h>

gboolean restraint_install_package(const gchar *package_name, GError **error);

#endif
