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

#ifndef _RESTRAINT_CONFIG_H
#define _RESTRAINT_CONFIG_H

gint64 restraint_config_get_int64 (gchar *config_file, gchar *section, gchar *key, GError **error);
guint64 restraint_config_get_uint64 (gchar *config_file, gchar *section, gchar *key, GError **error);
gboolean restraint_config_get_boolean (gchar *config_file, gchar *section, gchar *key, GError **error);
gchar *restraint_config_get_string (gchar *config_file, gchar *section, gchar *key, GError **error);
gchar **restraint_config_get_keys (gchar *config_file, gchar *section, GError **error);
void restraint_config_set (gchar *config_file, const gchar *section,
                           const gchar *key, GError **gerror, GType type, ...);
void restraint_config_trunc (gchar *config_file, GError **error);

#endif
