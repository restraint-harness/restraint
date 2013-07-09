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

void connections_close (AppData *app_data);
void connections_write (GList *connections, GString *s);
#endif
