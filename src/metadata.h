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


#define RESTRAINT_METADATA_PARSE_ERROR restraint_metadata_parse_error_quark()
#define VAR_LIB_PATH "/var/lib/restraint"
GQuark restraint_metadata_parse_error_quark(void);
typedef enum {
    RESTRAINT_METADATA_PARSE_ERROR_BAD_SYNTAX, /* parse errors from metadata*/
    RESTRAINT_METADATA_MISSING_FILE, /* Missing file*/
    RESTRAINT_METADATA_OPEN, /* Unable to open file*/
} RestraintMetadataParseError;

gboolean restraint_metadata_parse(Task *task, GError **error);
gboolean restraint_generate_testinfo(AppData *app_data, GError **error);
gboolean restraint_is_rhts_compat (Task *task);
gboolean restraint_no_testinfo (Task *task);
void restraint_parse_run_metadata (Task *task, GError **error);
void restraint_set_run_metadata (Task *task, gchar *key, GError **error, GType type, ...);
void restraint_set_running_config (gchar *key, gchar *value, GError **error);
gchar * restraint_get_running_config (gchar *key, GError **error);
guint64 parse_time_string(gchar *time_string, GError **error);
