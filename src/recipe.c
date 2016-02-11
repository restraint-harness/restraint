/*  
    This file is part of Restraint.

    Restraint is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Restraint is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Restraint.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <glib.h>
#include <gio/gio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "recipe.h"
#include "param.h"
#include "role.h"
#include "task.h"
#include "server.h"
#include "metadata.h"
#include "utils.h"
#include "config.h"
#include "xml.h"

xmlParserCtxt *ctxt = NULL;

const size_t buf_size = 4096;
gchar buf[4096];

GQuark restraint_recipe_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-recipe-parse-error-quark");
}

static gchar *get_attribute(xmlNode *node, void *attribute) {
    gchar *result;

    xmlChar *text = xmlGetNoNsProp(node, (xmlChar *)attribute);
    result = g_strdup((gchar *)text);
    xmlFree(text);
    return result;
}

static xmlNodePtr find_recipe(xmlDocPtr doc) {
    xmlXPathObjectPtr recipe_nodes;
    xmlNodePtr result = NULL;

    recipe_nodes = get_node_set (doc, NULL, (xmlChar *)"//recipe[@id]");
    if (recipe_nodes) {
        result = recipe_nodes->nodesetval->nodeTab[0];
        xmlXPathFreeObject (recipe_nodes);
        return result;
    }
    recipe_nodes = get_node_set (doc, NULL, (xmlChar *)"//guestrecipe");
    if (recipe_nodes) {
        result = recipe_nodes->nodesetval->nodeTab[0];
        xmlXPathFreeObject (recipe_nodes);
        return result;
    }
    return NULL;
}

#define unrecognised(message, ...) g_set_error(error, RESTRAINT_RECIPE_PARSE_ERROR, \
        RESTRAINT_RECIPE_PARSE_ERROR_UNRECOGNISED, \
        message, ##__VA_ARGS__)

static Param *parse_param(xmlNode *param_node, GError **error) {
    g_return_val_if_fail(param_node != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    Param *param = restraint_param_new();

    xmlChar *param_name = xmlGetNoNsProp(param_node, (xmlChar *)"name");
    if (param_name == NULL) {
        unrecognised("'param' element without 'name' attribute");
        goto error;
    }
    param->name = g_strdup((gchar *)param_name);
    xmlFree(param_name);

    xmlChar *param_value = xmlGetNoNsProp(param_node, (xmlChar *)"value");
    if (param_value == NULL) {
        unrecognised("'param' element without 'value' attribute");
        goto error;
    }
    param->value = g_strdup((gchar *)param_value);
    xmlFree(param_value);
    return param;

error:
    restraint_param_free(param);
    return NULL;
}

static GList *parse_params(xmlNode *params_node, GError **error) {

    GError *tmp_error = NULL;
    GList *params = NULL;
    xmlNode *child = params_node->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "param") == 0) {
            Param *param = parse_param(child, &tmp_error);
            /* Empty param element is an error */
            if (param == NULL) {
                g_propagate_error(error, tmp_error);
                goto error;
            }
            params = g_list_prepend(params, param);
        }
        child = child->next;
    }
    params = g_list_reverse(params);
    return params;
error:
    g_list_free_full(params, (GDestroyNotify) restraint_param_free);
    return NULL;
}

static GPtrArray *parse_role_system(xmlNode *system_node, GError **error) {
    GPtrArray *systems;
    systems = g_ptr_array_new_with_free_func((GDestroyNotify) g_free);

    xmlNode *child = system_node->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
           g_strcmp0((gchar *)child->name, "system") == 0) {
            xmlChar *system_value = xmlGetNoNsProp(child, (xmlChar *)"value");
            if (system_value == NULL) {
                unrecognised("'system' element without 'value' attribute");
                goto error;
            }
            g_ptr_array_add(systems, g_strdup((gchar *)system_value));
            xmlFree(system_value);
        }
        child = child->next;
    }
    g_ptr_array_add(systems, NULL);
    return systems;
error:
    g_ptr_array_free(systems, TRUE);
    return NULL;
}

static Role *parse_role(xmlNode *role_node, GError **error) {
    g_return_val_if_fail(role_node != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    GError *tmp_error = NULL;

    Role *role = restraint_role_new();

    xmlChar *role_value = xmlGetNoNsProp(role_node, (xmlChar *)"value");
    if (role_value == NULL) {
        unrecognised("'role' element without 'value' attribute");
        goto error;
    }
    role->value = g_strdup((gchar *)role_value);
    xmlFree(role_value);
    GPtrArray *systems = parse_role_system(role_node, &tmp_error);
    if (systems == NULL) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    role->systems = g_strjoinv (" ", (gchar **) systems->pdata);
    g_ptr_array_free(systems, TRUE);
    return role;

error:
    restraint_role_free(role);
    return NULL;
}

static GList *parse_roles(xmlNode *roles_node, GError **error) {

    GError *tmp_error = NULL;
    GList *roles = NULL;
    xmlNode *child = roles_node->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "role") == 0) {
            Role *role = parse_role(child, &tmp_error);
            /* Empty role element is an error */
            if (role == NULL) {
                g_propagate_error(error, tmp_error);
                goto error;
            }
            roles = g_list_prepend(roles, role);
        }
        child = child->next;
    }
    roles = g_list_reverse(roles);
    return roles;
error:
    g_list_free_full(roles, (GDestroyNotify) restraint_role_free);
    return NULL;
}

static void
check_param_max_time (Param *param, Task *task)
{
    if (g_strcmp0 (param->name, "KILLTIMEOVERRIDE") == 0 ||
        g_strcmp0 (param->name, "RSTRNT_MAX_TIME") == 0) {
        task->remaining_time = parse_time_string (param->value, NULL);
    }
}

static Task *parse_task(xmlNode *task_node, Recipe *recipe, GError **error) {
    g_return_val_if_fail(task_node != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    GError *tmp_error = NULL;

    Task *task = restraint_task_new();
    g_return_val_if_fail(task != NULL, NULL);

    task->recipe = recipe;
    xmlChar *task_name = xmlGetNoNsProp(task_node, (xmlChar *)"name");
    task->name = g_strdup((gchar *)task_name);
    xmlFree (task_name);

    xmlChar *keepchanges = xmlGetNoNsProp(task_node, (xmlChar *)"keepchanges");
    if (g_strcmp0((gchar *)keepchanges, "yes") == 0) {
        task->keepchanges = TRUE;
    }
    xmlFree(keepchanges);

    xmlChar *task_id = xmlGetNoNsProp(task_node, (xmlChar *)"id");
    if (task_id == NULL) {
        unrecognised("<task/> without id");
        goto error;
    }
    task->task_id = g_strdup((gchar *)task_id);
    xmlFree(task_id);

    gchar *suffix = g_strconcat("tasks/", task->task_id, "/", NULL);
    task->task_uri = soup_uri_new_with_base(recipe->recipe_uri, suffix);
    g_free(suffix);

    xmlNode *fetch = first_child_with_name(task_node, "fetch", FALSE);
    if (fetch != NULL) {
        task->fetch_method = TASK_FETCH_UNPACK;
        xmlChar *url = xmlGetNoNsProp(fetch, (xmlChar *)"url");
        if (url == NULL) {
            unrecognised("Task %s has 'fetch' element with 'url' attribute",
                    task->task_id);
            goto error;
        }
        task->fetch.url = soup_uri_new((char *)url);
        xmlFree(url);
        task->path = g_build_filename(task->recipe->base_path,
                soup_uri_get_host(task->fetch.url),
                soup_uri_get_path(task->fetch.url),
                soup_uri_get_fragment(task->fetch.url),
                NULL);
    } else {
        task->fetch_method = TASK_FETCH_INSTALL_PACKAGE;
        xmlNode *rpm = first_child_with_name(task_node, "rpm", FALSE);
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
        xmlChar *rpm_path = xmlGetNoNsProp(rpm, (xmlChar *)"path");
        if (rpm_path == NULL) {
            unrecognised("Task %s has 'rpm' element without 'path' attribute",
                    task->task_id);
            goto error;
        }
        task->path = g_strdup((gchar *)rpm_path);
        xmlFree(rpm_path);
    }

    xmlNode *params_node = first_child_with_name(task_node, "params", FALSE);
    if (params_node != NULL) {
        task->params = parse_params(params_node, &tmp_error);
        /* params could be empty, but if parsing causes an error then fail */
        if (tmp_error != NULL) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s has ", task->task_id);
            goto error;
        }
        /* Task param can override max_time */
        g_list_foreach (task->params, (GFunc) check_param_max_time, task);
    }

    xmlNode *roles_node = first_child_with_name(task_node, "roles", FALSE);
    if (roles_node != NULL) {
        task->roles = parse_roles(roles_node, &tmp_error);
        /* roles could be empty, but if parsing causes an error then fail */
        if (tmp_error != NULL) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s has ", task->task_id);
            goto error;
        }
    }

    xmlChar *status = xmlGetNoNsProp(task_node, (xmlChar *)"status");
    if (g_strcmp0((gchar *)status, "Running") == 0) {
        // We can't rely on the server because it "starts" the first task 
        // If we pay attention to that then we won't install the task
        // Update watchdog or install dependencies.
        g_warning ("Ignoring Server Running state\n");
        //task->started = TRUE;
    } else if (g_strcmp0((gchar *)status, "Completed") == 0 ||
            g_strcmp0((gchar *)status, "Aborted") == 0 ||
            g_strcmp0((gchar *)status, "Cancelled") == 0) {
        task->started = TRUE;
        task->finished = TRUE;
    }
    xmlFree(status);

    return task;

error:
    restraint_task_free(task);
    return NULL;
}

void restraint_recipe_free(Recipe *recipe) {
    g_return_if_fail(recipe != NULL);
    g_free(recipe->recipe_id);
    g_free(recipe->job_id);
    g_free(recipe->recipe_set_id);
    g_free(recipe->osdistro);
    g_free(recipe->osmajor);
    g_free(recipe->osvariant);
    g_free(recipe->osarch);
    g_free(recipe->owner);
    soup_uri_free(recipe->recipe_uri);
    g_list_free_full(recipe->tasks, (GDestroyNotify) restraint_task_free);
    g_list_free_full(recipe->params, (GDestroyNotify) restraint_param_free);
    g_list_free_full(recipe->roles, (GDestroyNotify) restraint_role_free);
    g_slice_free(Recipe, recipe);
}

static Recipe *
recipe_parse (xmlDoc *doc, SoupURI *recipe_uri, GError **error)
{
    g_return_val_if_fail(doc != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GError *tmp_error = NULL;
    gint i = 0;
    Recipe *result = g_slice_new0(Recipe);

    xmlNode *job = xmlDocGetRootElement(doc);
    if (job == NULL) {
        unrecognised("Invalid XML Document");
        goto error;
    }

    xmlNodePtr recipe = find_recipe(doc);
    if (recipe == NULL) {
        unrecognised("<recipe/> element not found");
        goto error;
    }

    result->job_id = get_attribute(recipe, "job_id");
    result->recipe_set_id = get_attribute(recipe, "recipe_set_id");
    result->recipe_id = get_attribute(recipe, "id");
    // Hack to make soup_uri_new happy.
    if (! recipe_uri) {
        gchar *recipe_url = g_strdup_printf ("http://localhost/recipes/%s/", result->recipe_id);
        recipe_uri = soup_uri_new (recipe_url);
        g_free (recipe_url);
    }
    result->osarch = get_attribute(recipe, "arch");
    result->osdistro = get_attribute(recipe, "distro");
    result->osmajor = get_attribute(recipe, "family");
    result->osvariant = get_attribute(recipe, "variant");
    result->owner = get_attribute(job, "owner");
    result->recipe_uri = recipe_uri;
    result->base_path = TASK_LOCATION;

    GList *tasks = NULL;
    xmlNode *child = recipe->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "task") == 0) {
            Task *task = parse_task(child, result, &tmp_error);
            if (task == NULL) {
                g_propagate_error(error, tmp_error);
                goto error;
            }
            /* link task to recipe for additional attributes */
            task->recipe = result;
            task->order = i++;
            tasks = g_list_prepend(tasks, task);
        } else
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "params") == 0) {
            result->params = parse_params(child, &tmp_error);
            /* params could be empty, but if parsing causes an error then fail */
            if (tmp_error != NULL) {
                g_propagate_prefixed_error(error, tmp_error,
                                 "Recipe %s has ", result->recipe_id);
                goto error;
            }
        } else
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "roles") == 0) {
            result->roles = parse_roles(child, &tmp_error);
            /* roles could be empty, but if parsing causes an error then fail */
            if (tmp_error != NULL) {
                g_propagate_prefixed_error(error, tmp_error,
                                 "Recipe %s has ", result->recipe_id);
                goto error;
            }
        }
        child = child->next;
    }
    tasks = g_list_reverse(tasks);

    result->tasks = tasks;
    return result;

error:
    restraint_recipe_free(result);
    return NULL;
}

static void
read_finish (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source);
    AppData *app_data = (AppData *) user_data;
    gssize size;

    size = g_input_stream_read_finish (stream, result, &app_data->error);

    if (app_data->error || size < 0) {
        g_input_stream_close (stream, NULL, NULL);
        g_object_unref(stream);
        app_data->state = RECIPE_COMPLETE;
        return;
    } else if (size == 0) {
        // Finished Reading
        g_input_stream_close (stream, NULL, NULL);
        g_object_unref(stream);
        xmlParserErrors xmlresult = xmlParseChunk(ctxt, buf, 0, /* terminator */ 1);
        if (xmlresult != XML_ERR_OK) {
            xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
            g_set_error_literal(&app_data->error, RESTRAINT_RECIPE_PARSE_ERROR,
                    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                    xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
            xmlFreeParserCtxt(ctxt);
            ctxt = NULL;
            /* Set us back to idle so we can accept a valid recipe */
            app_data->state = RECIPE_COMPLETE;
            return;
        }
        app_data->state = RECIPE_PARSE;
        return;
    } else {
        // Read
        if (!ctxt) {
            ctxt = xmlCreatePushParserCtxt(NULL, NULL, buf, size, app_data->recipe_url);
            if (ctxt == NULL) {
                g_critical("xmlCreatePushParserCtxt failed");
                /* Set us back to idle so we can accept a valid recipe */
                app_data->state = RECIPE_COMPLETE;
                return;
            }
        } else {
            xmlParserErrors xmlresult = xmlParseChunk(ctxt, buf, size,
                    /* terminator */ 0);
            if (xmlresult != XML_ERR_OK) {
                xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
                g_set_error_literal(&app_data->error, RESTRAINT_RECIPE_PARSE_ERROR,
                        RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                        xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
                xmlFreeDoc(ctxt->myDoc);
                xmlFreeParserCtxt(ctxt);
                ctxt = NULL;
                g_input_stream_close (stream, NULL, NULL);
                g_object_unref(stream);
                /* Set us back to idle so we can accept a valid recipe */
                app_data->state = RECIPE_COMPLETE;
                return;
            }
        }
        g_input_stream_read_async (stream,
                                   buf,
                                   buf_size,
                                   G_PRIORITY_DEFAULT,
                                   /* cancellable */ NULL,
                                   read_finish, user_data);
    }

}

static gboolean fetch_retry (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    g_clear_error(&app_data->error);
    app_data->state = RECIPE_FETCH;

    return FALSE;
}

void
restraint_recipe_parse_stream (GInputStream *stream, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    if (!stream) {
        if (app_data->fetch_retries < FETCH_RETRIES) {
            g_print("* RETRY [%d]**:%s\n", ++app_data->fetch_retries,
                    app_data->error->message);
            g_timeout_add_seconds(FETCH_INTERVAL, fetch_retry, app_data);
        } else {
            app_data->state = RECIPE_COMPLETE;
        }

        return;
    }
    g_input_stream_read_async (stream,
                               buf,
                               buf_size,
                               G_PRIORITY_DEFAULT,
                               /* cancellable */ NULL,
                               read_finish, user_data);
    return;
}

void
restraint_recipe_parse_request (GObject *source, GAsyncResult *res, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GInputStream *stream;

    stream = soup_request_send_finish (SOUP_REQUEST (source), res, &app_data->error);
    restraint_recipe_parse_stream (stream, user_data);
    return;
}

gboolean
recipe_handler (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    SoupURI *recipe_uri = NULL;
    SoupRequest *request;
    GString *message = g_string_new(NULL);
    gboolean result = TRUE;

    switch (app_data->state) {
        case RECIPE_FETCH:
            g_string_printf(message, "* Fetching recipe: %s\n", app_data->recipe_url);
            app_data->state = RECIPE_FETCHING;
            request = soup_session_request(soup_session, app_data->recipe_url, &app_data->error);
            // restraint_recipe_parse_xml will move us to the next state
            if (app_data->error) {
                app_data->state = RECIPE_COMPLETE;
            } else {
                soup_request_send_async(request,
                                        /* cancellable */ NULL,
                                        restraint_recipe_parse_request,
                                        app_data);
            }
            g_object_unref (request);
            break;
        case RECIPE_PARSE:
            g_string_printf(message, "* Parsing recipe\n");
            if (app_data->recipe_url)
                recipe_uri = soup_uri_new(app_data->recipe_url);
            app_data->recipe = recipe_parse(ctxt->myDoc, recipe_uri, &app_data->error);
            xmlFreeDoc(ctxt->myDoc);
            xmlFreeParserCtxt(ctxt);
            ctxt = NULL;
            if (app_data->recipe && ! app_data->error) {
                app_data->tasks = app_data->recipe->tasks;
                app_data->state = RECIPE_RUN;
            } else {
                app_data->state = RECIPE_COMPLETE;
            }
            break;
        case RECIPE_RUN:
            if (app_data->recipe_url) {
                restraint_config_set (app_data->config_file,
                                      "restraint",
                                      "recipe_url",
                                      NULL,
                                      G_TYPE_STRING,
                                      app_data->recipe_url);
            }
            app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                        task_handler,
                                                        app_data,
                                                        NULL);
            g_string_printf(message, "* Running recipe\n");
            app_data->state = RECIPE_RUNNING;
            break;
        case RECIPE_RUNNING:
            result = FALSE;
            break;
        case RECIPE_COMPLETE:
            if (app_data->error) {
                g_string_printf(message, "* WARNING **:%s\n", app_data->error->message);
            } else {
                g_string_printf(message, "* Finished recipe\n");
                if (app_data->close_message && app_data->message_data) {
                    app_data->close_message (app_data->message_data);
                }
            }
            // free current recipe
            if (app_data->recipe) {
              restraint_recipe_free(app_data->recipe);
              app_data->recipe = NULL;
              g_free (app_data->recipe_url);
              app_data->recipe_url = NULL;
            }

            // We are done. remove ourselves so we can run another recipe.
            app_data->state = RECIPE_IDLE;
            result = FALSE;
            if (app_data->io_handler_id != 0) {
                g_source_remove (app_data->io_handler_id);
                app_data->io_handler_id = 0;
            }
            g_cancellable_reset (app_data->cancellable);
            break;
        default:
            break;
    }

    // write message out to stderr
    if (message->len) {
      if (fwrite(message->str, sizeof(gchar), message->len, stderr) != message->len)
          g_warning ("failed to write message");
    }

    g_string_free(message, TRUE);
    return result;
}
