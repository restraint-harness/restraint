
typedef enum {
    FETCH_INSTALL_PACKAGE,
    FETCH_UNPACK,
} RecipeTaskFetchMethod;

typedef struct {
    gchar *task_id;
    gchar *name;
    RecipeTaskFetchMethod fetch_method;
    union {
        gchar *package_name; // when FETCH_INSTALL_PACKAGE
        gchar *url; // when FETCH_UNPACK
    } fetch;
} RecipeTask;

typedef struct {
    gchar *recipe_id;
    GList *tasks; // list of RecipeTask *
} Recipe;

#define RESTRAINT_RECIPE_PARSE_ERROR restraint_recipe_parse_error_quark()
GQuark restraint_recipe_parse_error_quark(void);
typedef enum {
    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX, /* parse errors from libxml2 */
    RESTRAINT_RECIPE_PARSE_ERROR_UNRECOGNISED, /* xml structure not as we expected */
} RestraintRecipeParseError;

Recipe *restraint_parse_recipe(GFile *recipe_file, GError **error);
void restraint_free_recipe(Recipe *recipe);
