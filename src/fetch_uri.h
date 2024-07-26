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

#ifndef _RESTRAINT_FETCH_URI_H
#define _RESTRAINT_FETCH_URI_H

#include <libsoup/soup.h>

void restraint_fetch_uri(SoupURI *url,
                     const gchar *base_path,
                     gboolean keepchanges,
                     gboolean ssl_verify,
                     gboolean abort_recipe_on_fail,
                     ArchiveEntryCallback entry_callback,
                     FetchFinishCallback finish_callback,
                     gpointer user_data);

#endif
