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
#define DEFAULT_PORT 8081

struct _AppData;

typedef struct {
    gchar *address;
    xmlNodePtr recipe_node_ptr;
    GHashTable *tasks;
    gint recipe_id;
    SoupURI *remote_uri;
    SoupMessage *remote_msg;
    guint port;
    SoupServer *server;
    struct _AppData *app_data;
    GString *body;
} RecipeData;

typedef struct _AppData {
    GError *error;
    GMainLoop *loop;
    xmlDocPtr xml_doc;
    gchar *run_dir;
    const gchar *address;
    GHashTable *result_states_to;
    GHashTable *recipes;
    gint verbose;
    guint port;
    SoupAddressFamily address_family;
} AppData;
