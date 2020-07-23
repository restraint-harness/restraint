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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libsoup/soup.h>

#include "beaker_harness.h"

#define BKR_ENV_FILE "/etc/profile.d/beaker-harness-env.sh"

#define STREMPTY(str) (str == NULL || strlen (str) == 0)

/*
 * Checks if Beaker environment exists
 *
 * Note that there is no guarantee that the environment is valid or
 * belongs to the current instance.
 *
 * Returns non-zero if Beaker environment exists, 0 otherwise.
 */
int
rstrnt_bkr_env_exists (void)
{
    return (int) g_file_test (BKR_ENV_FILE, G_FILE_TEST_EXISTS);
}

/*
 * Checks if recipe_url is reachable and points to a valid recipe
 * endpoint.
 *
 * Note that this is not checking the recipe's actual content.
 *
 * Returns BKR_HEALTH_STATUS_GOOD if the response passed the health
 * criteria, BKR_HEALTH_STATUS_BAD if the response didn't pass the
 * health criteria, or BKR_HEALTH_STATUS_UNKNOWN if no response was
 * received.
 */
bkr_health_status_t
rstrnt_bkr_check_recipe (const char *recipe_url)
{
    SoupStatus              status;
    g_autoptr (SoupMessage) msg = NULL;
    g_autoptr (SoupSession) session = NULL;

    g_return_val_if_fail (!STREMPTY (recipe_url), BKR_HEALTH_STATUS_UNKNOWN);

    msg = soup_message_new ("HEAD", recipe_url);

    /* msg will be NULL if the URL cannot be parsed */
    g_return_val_if_fail (msg != NULL, BKR_HEALTH_STATUS_UNKNOWN);

    session = soup_session_new_with_options ("user-agent", "restraint",
                                             "ssl-strict", FALSE,
                                             NULL);

    status = soup_session_send_message (session, msg);

    if (SOUP_STATUS_IS_SUCCESSFUL (status)
        && soup_message_headers_header_equals (msg->response_headers, "Content-Type", "application/xml")
        && soup_message_headers_get_content_length (msg->response_headers) > 0)
        return BKR_HEALTH_STATUS_GOOD;

    if (status == SOUP_STATUS_NONE || SOUP_STATUS_IS_TRANSPORT_ERROR (status))
        return BKR_HEALTH_STATUS_UNKNOWN;

    return BKR_HEALTH_STATUS_BAD;
}
