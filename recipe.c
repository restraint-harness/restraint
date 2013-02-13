
#include <glib.h>
#include <gio/gio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "recipe.h"

GQuark restraint_recipe_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-recipe-parse-error-quark");
}

static xmlDoc *parse_xml_from_gfile(GFile *recipe_file, GError **error) {
    g_return_val_if_fail(recipe_file != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GError *tmp_error = NULL;
    GInputStream *in = G_INPUT_STREAM(g_file_read(recipe_file,
            /* cancellable */ NULL, &tmp_error));
    if (in == NULL) {
        g_propagate_error(error, tmp_error);
        return NULL;
    }

    const size_t buf_size = 4096;
    char buf[buf_size];

    gssize read = g_input_stream_read(in, buf, buf_size, /* cancellable */ NULL,
            &tmp_error);
    if (read < 0) {
        g_propagate_error(error, tmp_error);
        g_object_unref(in);
        return NULL;
    }

    char *recipe_filename = g_file_get_path(recipe_file);
    xmlParserCtxt *ctxt = xmlCreatePushParserCtxt(NULL, NULL, buf, read,
            recipe_filename);
    g_free(recipe_filename);
    if (ctxt == NULL) {
        g_critical("xmlCreatePushParserCtxt failed");
        g_object_unref(in);
        return NULL;
    }

    while (read > 0) {
        read = g_input_stream_read(in, buf, buf_size, /* cancellable */ NULL,
                &tmp_error);
        if (read < 0) {
            g_propagate_error(error, tmp_error);
            xmlFreeParserCtxt(ctxt);
            g_object_unref(in);
            return NULL;
        }

        xmlParserErrors result = xmlParseChunk(ctxt, buf, read,
                /* terminator */ 0);
        if (result != XML_ERR_OK) {
            xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
            g_set_error_literal(error, RESTRAINT_RECIPE_PARSE_ERROR,
                    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                    xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
            g_object_unref(in);
            xmlFreeDoc(ctxt->myDoc);
            xmlFreeParserCtxt(ctxt);
            g_object_unref(in);
            return NULL;
        }
    }

    g_object_unref(in);
    xmlParserErrors result = xmlParseChunk(ctxt, buf, 0, /* terminator */ 1);
    if (result != XML_ERR_OK) {
        xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
        g_set_error_literal(error, RESTRAINT_RECIPE_PARSE_ERROR,
                RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
        xmlFreeDoc(ctxt->myDoc);
        xmlFreeParserCtxt(ctxt);
        return NULL;
    }

    xmlDoc *doc = ctxt->myDoc;
    xmlFreeParserCtxt(ctxt);
    return doc;
}

static xmlNode *first_child_with_name(xmlNode *parent, const gchar *name) {
    xmlNode *child = parent->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, name) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}

#define unrecognised(message, ...) g_set_error(error, RESTRAINT_RECIPE_PARSE_ERROR, \
        RESTRAINT_RECIPE_PARSE_ERROR_UNRECOGNISED, \
        message, ##__VA_ARGS__)

static RecipeTask *parse_task(xmlNode *task_node, GError **error) {
    g_return_val_if_fail(task_node != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    RecipeTask *task = g_slice_new(RecipeTask);

    xmlChar *task_id = xmlGetNoNsProp(task_node, (xmlChar *)"id");
    if (task_id == NULL) {
        unrecognised("<task/> without id");
        goto error;
    }
    task->task_id = g_strdup((gchar *)task_id);
    xmlFree(task_id);

    xmlChar *name = xmlGetNoNsProp(task_node, (xmlChar *)"name");
    if (name == NULL) {
        unrecognised("Task %s missing 'name' attribute", task->task_id);
        goto error;
    }
    task->name = g_strdup((gchar *)name);
    xmlFree(name);

    xmlChar *url = xmlGetNoNsProp(task_node, (xmlChar *)"url");
    if (url != NULL) {
        task->fetch_method = FETCH_UNPACK;
        task->fetch.url = g_strdup((gchar *)url);
        xmlFree(url);
    } else {
        task->fetch_method = FETCH_INSTALL_PACKAGE;
        xmlNode *rpm = first_child_with_name(task_node, "rpm");
        if (rpm == NULL) {
            unrecognised("Task %s has neither 'url' attribute nor 'rpm' element",
                    task->task_id);
            goto error;
        }
        xmlChar *rpm_name = xmlGetNoNsProp(rpm, (xmlChar *)"name");
        if (rpm_name == NULL) {
            unrecognised("Task %s has 'rpm' element without 'name' attribute",
                    task->task_id);
            goto error;
        }
        task->fetch.package_name = g_strdup((gchar *)rpm_name);
        xmlFree(rpm_name);
    }

    return task;

error:
    g_slice_free(RecipeTask, task);
    return NULL;
}

Recipe *restraint_parse_recipe(GFile *recipe_file, GError **error) {
    g_return_val_if_fail(recipe_file != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GError *tmp_error = NULL;
    xmlDoc *doc = NULL;

    doc = parse_xml_from_gfile(recipe_file, &tmp_error);
    if (doc == NULL) {
        g_propagate_error(error, tmp_error);
        goto error;
    }

    xmlNode *job = xmlDocGetRootElement(doc);
    if (job == NULL || g_strcmp0((gchar *)job->name, "job") != 0) {
        unrecognised("<job/> element not found");
        goto error;
    }
    xmlNode *recipeset = first_child_with_name(job, "recipeSet");
    if (recipeset == NULL) {
        unrecognised("<recipeSet/> element not found");
        goto error;
    }
    xmlNode *recipe = first_child_with_name(recipeset, "recipe");
    if (recipe == NULL) {
        unrecognised("<recipe/> element not found");
        goto error;
    }
    // XXX guestrecipe?

    GList *tasks = NULL;
    xmlNode *child = recipe->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "task") == 0) {
            RecipeTask *task = parse_task(child, &tmp_error);
            if (task == NULL) {
                g_propagate_error(error, tmp_error);
                goto error;
            }
            tasks = g_list_prepend(tasks, task);
        }
        child = child->next;
    }
    tasks = g_list_reverse(tasks);

    Recipe *result = g_slice_new(Recipe);
    xmlChar *recipe_id = xmlGetNoNsProp(recipe, (xmlChar *)"id");
    result->recipe_id = g_strdup((gchar *)recipe_id);
    xmlFree(recipe_id);
    result->tasks = tasks;
    xmlFreeDoc(doc);
    return result;

error:
    if (doc != NULL)
        xmlFreeDoc(doc);
    return NULL;
}

static void restraint_free_recipe_task(RecipeTask *task) {
    g_free(task->task_id);
    g_free(task->name);
    switch (task->fetch_method) {
        case FETCH_INSTALL_PACKAGE:
            g_free(task->fetch.package_name);
            break;
        case FETCH_UNPACK:
            g_free(task->fetch.url);
            break;
        default:
            g_return_if_reached();
    }
    g_slice_free(RecipeTask, task);
}

void restraint_free_recipe(Recipe *recipe) {
    g_free(recipe->recipe_id);
    g_list_free_full(recipe->tasks, (GDestroyNotify) restraint_free_recipe_task);
    g_slice_free(Recipe, recipe);
}
