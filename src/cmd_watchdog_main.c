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
#include <glib.h>
#include "cmd_watchdog.h"
#include "errors.h"

int main(int argc, char *argv[]) {

    GError *error = NULL;
    WatchdogAppData *app_data = g_slice_new0 (WatchdogAppData);

    if (!parse_watchdog_arguments(app_data, argc, argv, &error)) {
        goto cleanup;
    }
    if(!upload_watchdog(app_data, &error)){
        goto cleanup;
    }

cleanup:

    clear_server_data(&app_data->s);

    g_slice_free(WatchdogAppData, app_data);

    if (error) {
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        g_clear_error(&error);

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
