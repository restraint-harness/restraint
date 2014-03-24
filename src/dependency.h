#include <glib.h>
#include "recipe.h"
#include "server.h"

typedef struct {
    AppData *app_data;
    GList *dependencies;
} DependencyData;

void restraint_install_dependencies (AppData *app_data);
