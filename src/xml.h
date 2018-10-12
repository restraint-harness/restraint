
#ifndef _RESTRAINT_XML_H
#define _RESTRAINT_XML_H

#define RNG_SCHEMA "/usr/share/restraint/client/job_schema.rng"

#define RESTRAINT_XML_PARSE_ERROR restraint_xml_parse_error_quark()
GQuark restraint_xml_parse_error_quark(void);
typedef enum {
    RESTRAINT_XML_PARSE_ERROR_BAD_SYNTAX, /* parse errors from libxml2 */
} RestraintXmlParseError;

typedef void (*RestraintXmlRequestCompletionCallback)(GError *error, xmlDoc *doc, gpointer user_data);

/**
 * restraint_xml_parse_stream:
 * @stream (transfer full): an input stream containing XML to be read and parsed.
 * @url: the URL from which the XML was retrieved.
 * @completion_callback: a function which will be called when the stream has
 *   been read to completion and the XML has been parsed (or an error occurred).
 * @user_data (closure): extra argument for the completion callback.
 *
 * Reads from the given stream asynchronously and parses the XML it contains,
 * producing an xmlDoc from it.
 *
 * The callback function will be called with the resulting xmlDoc and %NULL
 * for the error argument, if the document was parsed successfully. The
 * callback takes ownership of the xmlDoc.
 *
 * If an error occurred, the callback will be called with an error and %NULL
 * for the doc argument.
 */
void restraint_xml_parse_from_stream(
    GInputStream *stream,
    const gchar *url,
    RestraintXmlRequestCompletionCallback completion_callback,
    gpointer user_data);

void restraint_xml_parse_from_url(
    SoupSession *soup_session,
    const gchar *url,
    RestraintXmlRequestCompletionCallback completion_callback,
    gpointer user_data);

xmlNodePtr
first_child_with_name(xmlNodePtr parent_ptr, const gchar *name, gboolean create);
xmlXPathObjectPtr
get_node_set(xmlDocPtr doc, xmlNodePtr node, xmlChar *xpath);

#endif
