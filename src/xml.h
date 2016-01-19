#define RNG_SCHEMA "/usr/share/restraint/client/job_schema.rng"

xmlNodePtr
first_child_with_name(xmlNodePtr parent_ptr, const gchar *name, gboolean create);
xmlXPathObjectPtr
get_node_set(xmlDocPtr doc, xmlNodePtr node, xmlChar *xpath);
