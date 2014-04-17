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


#include <glib.h>
#include "role.h"

Role *restraint_role_new(void) {
    return g_slice_new0(Role);
}

void restraint_role_free(Role *role) {
    if (role->value != NULL)
        g_free(role->value);
    if (role->systems != NULL)
        g_free(role->systems);
    g_slice_free(Role, role);
}

