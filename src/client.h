
#define READ_BUFFER_SIZE 131072
#define SERVER_PORT 8000

typedef struct {
    GError *error;
    GMainLoop *loop;
    xmlDocPtr xml_doc;
    gchar *run_dir;
    const gchar *address;
    xmlNodePtr recipe_node_ptr;
    GHashTable *tasks;
    GHashTable *result_states_to;
    SoupURI *remote_uri;
    gint recipe_id;
    gint verbose;
} AppData;
