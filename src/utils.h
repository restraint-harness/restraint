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

#ifndef __UTILS_H
#define __UTILS_H

#include <glib.h>
#define BASE10 10

#define CMD_ENV_DIR "/var/lib/restraint"
#define CMD_ENV_FILE_FORMAT "%s/rstrnt-commands-env-%d.sh"

void update_env_script(gchar *prefix, gchar *restraint_url,
                       gchar *recipe_id, gchar *task_id,
                       GError **error);
gchar *get_envvar_filename(gint pid);
guint64 parse_time_string (gchar *time_string, GError **error);
gboolean file_exists (gchar *filename);
gchar *get_package_version(gchar *pkg_name, GError **error);

#endif
