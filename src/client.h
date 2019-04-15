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

#ifndef _CLIENT_H
#define _CLIENT_H

#include <libxml/parser.h>
#include <regex.h>
#include <json.h>

#define DEFAULT_DELAY 60
#define CONN_RETRIES 15

struct _AppData;

typedef void (*RegexCallback) (const char *path,
                               GHashTable *headers,
                               struct json_object *body,
                               gpointer user_data);

typedef struct {
    xmlNodePtr recipe_node_ptr;
    GHashTable *tasks;
    guint recipe_id;
    struct _AppData *app_data;
    GString *body;
    GCancellable *cancellable;
    guint timeout_handler_id;
    gchar *rhost;
    gchar *connect_uri;
} RecipeData;

typedef struct {
    regex_t regex;
    RegexCallback callback;
} RegexData;

typedef struct _AppData {
    GError *error;
    GMainLoop *loop;
    xmlDocPtr xml_doc;
    gchar *run_dir;
    GHashTable *result_states_to;
    GHashTable *recipes;
    gint verbose;
    GCancellable *cancellable;
    gboolean started;
    GSList *regexes;
    guint conn_retries;
    guint max_retries;
    gchar *rsh_cmd;
    gchar *restraint_path;
} AppData;

#endif
