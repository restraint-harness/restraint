#ifndef CMD_UTILS_H__
#define CMD_UTILS_H__

typedef struct {
    guint port;
    gchar *server;
    gchar *server_recipe;
    gchar *task_id;
} ServerData;

void clear_server_data(ServerData *s_data);
void get_env_vars_and_format_ServerData(ServerData *s_data);
void get_env_vars_from_file(ServerData *s_data, GError **error);
void format_server_string(ServerData *s_data,
                       void (*format_server)(ServerData *s_data),
                       GError **error);
void set_envvar_from_file(guint pid, GError **error);
void unset_envvar_from_file(guint pid, GError **error);
gchar *get_taskid (void);
gchar *get_recipe_url (void);

void cmd_usage(GOptionContext *context);

#endif
