
#ifndef _RESTRAINT_RECIPE_H
#define _RESTRAINT_RECIPE_H

#include <glib.h>
#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>

typedef enum {
    RECIPE_FETCH,
    RECIPE_FETCHING,
    RECIPE_PARSE,
    RECIPE_RUNNING,
    RECIPE_FAIL,
} RecipeSetupState;

typedef struct {
    gchar *recipe_id;
    gchar *job_id;
    gchar *recipe_set_id;
    gchar *osdistro;
    gchar *osmajor;
    gchar *osvariant;
    gchar *osarch;
    GList *tasks; // list of Task *
    GList *params; // list of Params
    GList *roles; // list of Roles
} Recipe;

#define RESTRAINT_RECIPE_PARSE_ERROR restraint_recipe_parse_error_quark()
GQuark restraint_recipe_parse_error_quark(void);
typedef enum {
    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX, /* parse errors from libxml2 */
    RESTRAINT_RECIPE_PARSE_ERROR_UNRECOGNISED, /* xml structure not as we expected */
} RestraintRecipeParseError;

gboolean recipe_handler (gpointer user_data);
void recipe_finish (gpointer user_data);
void restraint_recipe_parse_xml (GObject *source, GAsyncResult *res, gpointer user_data);
void restraint_recipe_free(Recipe *recipe);

extern SoupRequester *soup_requester;
#endif
