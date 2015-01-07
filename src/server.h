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

#ifndef _RESTRAINT_SERVER_H
#define _RESTRAINT_SERVER_H

#define VAR_LIB_PATH "/var/lib/restraint"
#define PLUGIN_SCRIPT "/usr/share/restraint/plugins/run_plugins"
#define TASK_PLUGIN_SCRIPT "/usr/share/restraint/plugins/run_task_plugins"
#define PLUGIN_DIR "/usr/share/restraint/plugins"

typedef enum {
  ABORTED_NONE,
  ABORTED_RECIPE,
  ABORTED_TASK,
} StateAborted;

typedef struct {
  RecipeSetupState state;
  guint recipe_handler_id;
  guint task_handler_id;
  gchar *recipe_url;
  Recipe *recipe;
  GList *tasks;
  GError *error;
  gchar *config_file;
  gchar *restraint_url;
  GCancellable *cancellable;
  QueueMessage queue_message;
  CloseMessage close_message;
  gpointer message_data;
  guint finished_handler_id;
  guint io_handler_id;
  GIOChannel *io_chan;
  StateAborted aborted;
} AppData;

void connections_write (AppData *app_data, gchar *msg_data, gsize msg_len);
#endif
