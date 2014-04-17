/*  
    This file is part of Restraint.

    Restraint is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Restraint is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Restraint.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _RESTRAINT_PACKAGES_H
#define _RESTRAINT_PACKAGES_H

/*
 * Code for manipulating system packages.
 */

#include <glib.h>

gboolean restraint_install_package(AppData *app_data, const gchar *package_name, GError **error);

#endif
