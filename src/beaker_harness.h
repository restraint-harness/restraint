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

#ifndef _RESTRAINT_BEAKER_HARNESS_H
#define _RESTRAINT_BEAKER_HARNESS_H

#define BKR_ENV_EXISTS()           (rstrnt_bkr_env_exists () != 0)
#define BKR_RECIPE_IS_HEALTHY(url) (BKR_HEALTH_STATUS_GOOD == rstrnt_bkr_check_recipe (url))

typedef enum bkr_health_status {
    BKR_HEALTH_STATUS_UNKNOWN, /* No response from lab controller */
    BKR_HEALTH_STATUS_GOOD,    /* Expected response from lab controller */
    BKR_HEALTH_STATUS_BAD,     /* Un-expected response from lab controller */
} bkr_health_status_t;

bkr_health_status_t rstrnt_bkr_check_recipe (const char *recipe_url);

int rstrnt_bkr_env_exists (void);

#endif
