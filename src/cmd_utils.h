#ifndef _RESTRAINT_CMD_UTILS_H
#define _RESTRAINT_CMD_UTILS_H

#include <glib.h>

typedef struct {
    guint port;
    gchar *server;
    gchar *server_recipe;
    gchar *task_id;
} ServerData;

const gchar *rstrnt_getenv (const gchar *name);

#define get_taskid()     ((gchar *) rstrnt_getenv ("TASKID"))
#define get_recipe_url() ((gchar *) rstrnt_getenv ("RECIPE_URL"))

void clear_server_data(ServerData *s_data);
void get_env_vars_and_format_ServerData(ServerData *s_data);
void get_env_vars_from_file(ServerData *s_data, GError **error);
void format_server_string(ServerData *s_data,
                       void (*format_server)(ServerData *s_data),
                       GError **error);
void set_envvar_from_file(guint port, GError **error);
void unset_envvar_from_file(guint port, GError **error);

void cmd_usage(GOptionContext *context);

#endif
