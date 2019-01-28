#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

#include "xml.h"

GQuark restraint_xml_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-xml-parse-error-quark");
}

typedef struct {
    GError *error;
    char buf[4096];
    xmlParserCtxt *parser_ctxt;
    gchar *url;
    RestraintXmlRequestCompletionCallback completion_callback;
    gpointer completion_callback_user_data;
} RestraintXmlRequestContext;

static void
restraint_xml_read_callback(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GInputStream *stream = G_INPUT_STREAM(source);
    RestraintXmlRequestContext *ctxt = (RestraintXmlRequestContext *)user_data;
    gssize size;

    size = g_input_stream_read_finish(stream, result, &ctxt->error);

    if (size < 0) {
        g_warn_if_fail(ctxt->error);
    }

    if (ctxt->error) {
        goto finished;
    }

    // We only initialise the XML parsing context after we have read some
    // bytes, not sooner, because it uses the initial bytes for charset detection.
    if (ctxt->parser_ctxt == NULL) {
        ctxt->parser_ctxt = xmlCreatePushParserCtxt(NULL, NULL, ctxt->buf, size, ctxt->url);
        if (ctxt->parser_ctxt == NULL) {
            g_set_error_literal(&ctxt->error, RESTRAINT_XML_PARSE_ERROR,
                    RESTRAINT_XML_PARSE_ERROR_BAD_SYNTAX,
                    "Error creating libxml parser context");
            goto finished;
        }
    } else {
        xmlParserErrors xmlresult = xmlParseChunk(ctxt->parser_ctxt, ctxt->buf, size,
                size > 0 ? 0 : 1);
        if (xmlresult != XML_ERR_OK) {
            xmlError *xmlerr = xmlCtxtGetLastError(ctxt->parser_ctxt);
            g_set_error_literal(&ctxt->error, RESTRAINT_XML_PARSE_ERROR,
                    RESTRAINT_XML_PARSE_ERROR_BAD_SYNTAX,
                    xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
            goto finished;
        }
    }

    if (size > 0) {
        // Go back to read another chunk.
        g_input_stream_read_async(stream, ctxt->buf, sizeof(ctxt->buf),
                G_PRIORITY_DEFAULT, /* cancellable */ NULL,
                restraint_xml_read_callback, ctxt);
        return;
    }

finished:
    g_input_stream_close(stream, /* cancellable */ NULL, &ctxt->error);
    g_object_unref(stream);

    if (ctxt->error) {
        if (ctxt->parser_ctxt) {
            xmlFreeDoc(ctxt->parser_ctxt->myDoc);
            xmlFreeParserCtxt(ctxt->parser_ctxt);
        }
        ctxt->completion_callback(ctxt->error, NULL,
                ctxt->completion_callback_user_data);
    } else {
        g_warn_if_fail(ctxt->parser_ctxt != NULL);
        g_warn_if_fail(ctxt->parser_ctxt->myDoc != NULL);
        // If there was no error, we transfer ownership of myDoc to the callback.
        ctxt->completion_callback(NULL, ctxt->parser_ctxt->myDoc,
                ctxt->completion_callback_user_data);
        xmlFreeParserCtxt(ctxt->parser_ctxt);
    }

    g_clear_error(&ctxt->error);
    g_free(ctxt->url);
    g_slice_free(RestraintXmlRequestContext, ctxt);
}

void
restraint_xml_parse_from_stream(
    GInputStream *stream,
    const gchar *url,
    RestraintXmlRequestCompletionCallback completion_callback,
    gpointer user_data)
{
    g_return_if_fail(stream != NULL);
    g_return_if_fail(completion_callback != NULL);

    RestraintXmlRequestContext *ctxt = g_slice_new0(RestraintXmlRequestContext);
    ctxt->url = g_strdup(url);
    ctxt->completion_callback = completion_callback;
    ctxt->completion_callback_user_data = user_data;

    g_input_stream_read_async(stream, ctxt->buf, sizeof(ctxt->buf),
            G_PRIORITY_DEFAULT, /* cancellable */ NULL,
            restraint_xml_read_callback, ctxt);
}

static void
restraint_xml_request_callback(GObject *source, GAsyncResult *res, gpointer user_data)
{
    RestraintXmlRequestContext *ctxt = (RestraintXmlRequestContext *)user_data;

    GInputStream *stream = soup_request_send_finish(SOUP_REQUEST(source), res, &ctxt->error);
    if (!stream) {
        ctxt->completion_callback(ctxt->error, NULL, ctxt->completion_callback_user_data);
        return;
    }

    g_input_stream_read_async(stream, ctxt->buf, sizeof(ctxt->buf),
            G_PRIORITY_DEFAULT, /* cancellable */ NULL,
            restraint_xml_read_callback, ctxt);
}

void
restraint_xml_parse_from_url(
    SoupSession *soup_session,
    const gchar *url,
    RestraintXmlRequestCompletionCallback completion_callback,
    gpointer user_data)
{
    g_return_if_fail(soup_session != NULL);
    g_return_if_fail(url != NULL);
    g_return_if_fail(completion_callback != NULL);

    GError *error = NULL;

    SoupRequest *request = soup_session_request(soup_session, url, &error);
    if (!request) {
        completion_callback(error, NULL, user_data);
        return;
    }

    RestraintXmlRequestContext *ctxt = g_slice_new0(RestraintXmlRequestContext);
    ctxt->url = g_strdup(url);
    ctxt->completion_callback = completion_callback;
    ctxt->completion_callback_user_data = user_data;

    soup_request_send_async(request, /* cancellable */ NULL,
            restraint_xml_request_callback, ctxt);
    g_object_unref(request);
}

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
