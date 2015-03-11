#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

xmlNodePtr
first_child_with_name(xmlNodePtr parent_ptr, const gchar *name,
                      gboolean create)
{
    xmlNodePtr results_node_ptr = NULL;
    xmlNode *child = parent_ptr->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, name) == 0)
            return child;
        child = child->next;
    }
    // If requested create if not found
    if (create) {
        results_node_ptr = xmlNewTextChild (parent_ptr,
                                            NULL,
                                            (xmlChar *) name,
                                            NULL);
    }
    return results_node_ptr;
}

xmlXPathObjectPtr
get_node_set (xmlDocPtr doc, xmlNodePtr node, xmlChar *xpath)
{
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }

    if (node == NULL) {
        result = xmlXPathEvalExpression(xpath, context);
    } else {
        result = xmlXPathNodeEval(node, xpath, context);
    }
    xmlXPathFreeContext(context);
    if (result == NULL) {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        printf("No result\n");
        return NULL;
    }
    return result;
}

