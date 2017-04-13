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
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "errors.h"
#include "utils.h"

void cmd_usage(GOptionContext *context) {
    gchar *usage_str = g_option_context_get_help(context, FALSE, NULL);
    g_print("%s", usage_str);
    g_free(usage_str);
}

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
    gint read = sscanf(time_string, "%" G_GUINT64_FORMAT " %c", &max_time, &time_unit);
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
            g_set_error (error, RESTRAINT_ERROR,
                         RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                         "Unrecognised time unit '%c'", time_unit);
        }
    } else if (read == 1) {
        max_time = max_time;
    } else {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to parse time string: %s", time_string);
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

void restraint_free_url(struct restraint_url *rurl)
{
    g_free(rurl->uri);
    g_free(rurl->scheme);
    g_free(rurl->host);
    g_free(rurl->path);
    g_free(rurl->query);
    g_free(rurl->fragment);
    g_free(rurl);
}

struct restraint_url *restraint_copy_url(struct restraint_url *url)
{
    struct restraint_url *res = g_new0(struct restraint_url, 1);

    res->uri = g_strdup(url->uri);
    res->scheme = g_strdup(url->scheme);
    res->host = g_strdup(url->host);
    res->port = url->port;
    res->path = g_strdup(url->path);
    res->query = g_strdup(url->query);
    res->fragment = g_strdup(url->fragment);

    return res;
}

struct restraint_url *restraint_parse_url(gchar *url)
{
    GMatchInfo *minfo = NULL;
    struct restraint_url *res = NULL;
    GRegex *url_regex = g_regex_new("^([^:]+)://([^/]+)(:\\d+)?(/[^\\?#]*)?(\\?([^#]*))?(#(.*))?$",
                                    G_REGEX_CASELESS, 0, NULL);

    if (g_regex_match(url_regex, url, 0, &minfo)) {
        gchar *tmp = NULL;
        res = g_new0(struct restraint_url, 1);
        res->uri = g_strdup(url);
        res->scheme = g_match_info_fetch(minfo, 1);
        res->host = g_match_info_fetch(minfo, 2);

        tmp = g_match_info_fetch(minfo, 3);
        if (g_strcmp0(tmp, "") != 0) {
            res->port = atoi(tmp);
        }
        g_free(tmp);

        res->path = g_match_info_fetch(minfo, 4);
        if (g_strcmp0(res->path, "") == 0) {
            g_free(res->path);
            res->path = g_strdup("/");
        }

        res->query = g_match_info_fetch(minfo, 6);
        if (g_strcmp0(res->query, "") == 0) {
            g_free(res->query);
            res->query = NULL;
        }

        res->fragment = g_match_info_fetch(minfo, 8);
        if (g_strcmp0(res->fragment, "") == 0) {
            g_free(res->fragment);
            res->fragment = NULL;
        }
    }

    g_match_info_free(minfo);
    g_regex_unref(url_regex);
    return res;
}
