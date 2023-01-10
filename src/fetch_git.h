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

#ifndef _RESTRAINT_FETCH_GIT_H
#define _RESTRAINT_FETCH_GIT_H

#define GIT_PORT 9418
#define GIT_BRANCHES_DELIMITER ","
#define GIT_BRANCHES "main,master"
#define GIT_BRANCHES_SIZE 2
#define HDR_LEN_SIZE 4

#include <libsoup/soup.h>

void restraint_fetch_git (SoupURI *url,
                     const gchar *base_path,
                     gboolean keepchanges,
                     ArchiveEntryCallback entry_callback,
                     FetchFinishCallback finish_callback,
                     gpointer user_data);

#endif
