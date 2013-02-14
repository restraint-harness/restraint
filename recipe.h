
#ifndef _RESTRAINT_RECIPE_H
#define _RESTRAINT_RECIPE_H

#include <glib.h>

#include "task.h"

typedef struct {
    gchar *recipe_id;
    GList *tasks; // list of Task *
} Recipe;

#define RESTRAINT_RECIPE_PARSE_ERROR restraint_recipe_parse_error_quark()
GQuark restraint_recipe_parse_error_quark(void);
typedef enum {
    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX, /* parse errors from libxml2 */
    RESTRAINT_RECIPE_PARSE_ERROR_UNRECOGNISED, /* xml structure not as we expected */
} RestraintRecipeParseError;

Recipe *restraint_recipe_new_from_xml(GFile *recipe_file, GError **error);
void restraint_recipe_free(Recipe *recipe);

#endif
