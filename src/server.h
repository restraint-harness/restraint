#ifndef _RESTRAINT_SERVER_H
#define _RESTRAINT_SERVER_H

#define PLUGIN_SCRIPT "/usr/share/restraint/plugins/run_plugins"
#define TASK_PLUGIN_SCRIPT "/usr/share/restraint/plugins/run_task_plugins"
#define PLUGIN_DIR "/usr/share/restraint/plugins"

typedef struct {
  GMainLoop *loop;
  RecipeSetupState state;
  guint recipe_handler_id;
  guint task_handler_id;
  gchar *recipe_url;
  Recipe *recipe;
  GList *tasks;
  GError *error;
  SoupServer *soup_server;
  SoupMessage *client_msg;
} AppData;

typedef struct {
    const gchar *path;
    AppData *app_data;
    SoupMessage *client_msg;
    gchar **environ;
} ServerData;

void connections_write (AppData *app_data, gchar *msg_data, gsize msg_len);
#endif
