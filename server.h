#ifndef _RESTRAINT_SERVER_H
#define _RESTRAINT_SERVER_H

typedef struct {
  GMainLoop *loop;
  RecipeSetupState state;
  GList *connections;
  guint recipe_handler_id;
  guint task_handler_id;
  gchar *recipe_url;
  Recipe *recipe;
  GList *tasks;
  GError *error;
} AppData;

typedef struct {
  GSocketConnection *connection;
  guint watch_id;
  GIOChannel *channel;
} ClientConnection;

void connections_close (AppData *app_data, gboolean monitor);
void connections_write (AppData *app_data, GString *s, gint stream_type, gint status);
void connections_error (AppData *app_data, GError *error);
#endif
