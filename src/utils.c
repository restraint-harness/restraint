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

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "errors.h"
#include "utils.h"

guint64
parse_time_string(gchar *time_string, GError **error)
{
    /* Convert time string to number of seconds.
     *     5d -> 432000
     *     3m -> 180
     *     2h -> 7200
     *     600s -> 600
     */
    gchar time_unit;
    guint64 max_time = 0;
    gint read = sscanf(time_string, "%" SCNu64 " %c", &max_time, &time_unit);
    if (read == 2) {
        time_unit = g_ascii_toupper(time_unit);
        if (time_unit == 'D')
            max_time = 24 * 3600 * max_time;
        else if (time_unit == 'H')
            max_time = 3600 * max_time;
        else if (time_unit == 'M')
            max_time = 60 * max_time;
        else if (time_unit == 'S')
            max_time = max_time;
        else {
            g_set_error (error, RESTRAINT_PARSE_ERROR,
                         RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                         "Unrecognised time unit '%c'", time_unit);
        }
    }
    return max_time;
}

gboolean
file_exists (gchar *filename)
{
    /* Checking if a file exists before creating it can be
     * prone to race conditions.  Be careful.
     */

    GStatBuf stat_buf;
    if (g_stat (filename, &stat_buf) == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}
