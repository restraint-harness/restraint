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

#ifndef _RESTRAINT_UTILS_H
#define _RESTRAINT_UTILS_H

#include <glib.h>
#define BASE10 10

#define CMD_ENV_DIR "/var/lib/restraint"
#define CMD_ENV_FILE_FORMAT "%s/rstrnt-commands-env-%u.sh"

#define INSTALL_CONFIG_FILE "/var/lib/restraint/install_config"
#define INSTALL_DIR_VAR "INSTALL_DIR"
#define INSTALL_DIR_DEFAULT "/var/lib/restraint/tests"

#define STREQ(a, b) (g_strcmp0 (a, b) == 0)

void update_env_file(gchar *prefix, gchar *restraint_url,
                     gchar *recipe_id, gchar *task_id,
                     guint port, GError **error);
void remove_env_file(guint port);
gchar *get_envvar_filename(guint port);
guint64 parse_time_string (gchar *time_string, GError **error);
gboolean file_exists (gchar *filename);
gchar *get_package_version(gchar *pkg_name, GError **error);
gchar * get_install_dir(const gchar *filename, GError **error);

#endif
