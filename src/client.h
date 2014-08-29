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

#define READ_BUFFER_SIZE 131072

typedef struct {
    GError *error;
    GMainLoop *loop;
    xmlDocPtr xml_doc;
    gchar *run_dir;
    const gchar *address;
    xmlNodePtr recipe_node_ptr;
    GHashTable *tasks;
    GHashTable *result_states_to;
    GHashTable *recipe_hosts;
    SoupURI *remote_uri;
    gint recipe_id;
    gint verbose;
    GString *body;
    SoupMessage *remote_msg;
    guint port;
} AppData;
