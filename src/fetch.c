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

#define _XOPEN_SOURCE 500

#include "fetch.h"
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ftw.h>

GQuark restraint_fetch_libarchive_error(void) {
    return g_quark_from_static_string("restraint-fetch-libarchive-error-quark");
}

GQuark restraint_fetch_error(void) {
    return g_quark_from_static_string("restraint-fetch-error-quark");
}

static int unlink_cb(const char *fpath, const struct stat *sb,
                     int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        g_warning("Failed to remove %s", fpath);

    return rv;
}

int rmrf(const char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}
