
#include <glib.h>
#include <gio/gio.h>

#include "recipe.h"

static void run_task(Task *task, __attribute__((unused)) void *_) {
    switch (task->fetch_method) {
        case FETCH_UNPACK:
            g_print("Pretending to fetch and unpack %s\n", task->fetch.url);
            break;
        case FETCH_INSTALL_PACKAGE:
            g_print("Pretending to yum install %s\n", task->fetch.package_name);
            break;
        default:
            g_return_if_reached();
    }
    g_print("Pretending to run task %s\n", task->task_id);
}

int main(int argc, char *argv[]) {
    g_type_init();

    GError *error = NULL;
    gchar *run_recipe = NULL;
    GOptionEntry entries[] = {
        { "run-recipe", 0, 0, G_OPTION_ARG_STRING, &run_recipe,
            "Run recipe from URL or file", "URL" },
        { NULL }
    };
    GOptionContext *context = g_option_context_new("");
    g_option_context_set_summary(context,
            "Test harness for Beaker. Runs tasks according to a recipe \n"
            "and collects their results.");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);
    if (!parse_succeeded)
        goto error;
    if (run_recipe == NULL) {
        g_printerr("No recipe specified (use --run-recipe)\n");
        return 1;
    }

    GFile *recipe_file = g_file_new_for_commandline_arg(run_recipe);
    Recipe *recipe = restraint_recipe_new_from_xml(recipe_file, &error);
    g_object_unref(recipe_file);
    if (recipe == NULL)
        goto error;

    g_print("Pretending to run recipe %s\n", recipe->recipe_id);
    g_list_foreach(recipe->tasks, (GFunc) run_task, NULL);

    restraint_recipe_free(recipe);
    return 0;

error:
    g_printerr("%s [%s, %d]\n", error->message,
            g_quark_to_string(error->domain), error->code);
    return 1;
}
