
#include <glib.h>
#include <gio/gio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "recipe.h"
#include "param.h"
#include "role.h"
#include "task.h"
#include "common.h"
#include "server.h"

// XXX make this configurable
#define TASK_LOCATION "/mnt/tests"

xmlParserCtxt *ctxt = NULL;

const size_t buf_size = 4096;
gchar buf[4096];

GQuark restraint_recipe_parse_error_quark(void) {
    return g_quark_from_static_string("restraint-recipe-parse-error-quark");
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

static gchar *get_attribute(xmlNode *node, void *attribute) {
    gchar *result;

    xmlChar *text = xmlGetNoNsProp(node, (xmlChar *)attribute);
    result = g_strdup((gchar *)text);
    xmlFree(text);
    return result;
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

static Task *parse_task(xmlNode *task_node, SoupURI *recipe_uri, GError **error) {
    g_return_val_if_fail(task_node != NULL, NULL);
    g_return_val_if_fail(recipe_uri != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    GError *tmp_error = NULL;

    Task *task = restraint_task_new();
    g_return_val_if_fail(task != NULL, NULL);

    xmlChar *task_id = xmlGetNoNsProp(task_node, (xmlChar *)"id");
    if (task_id == NULL) {
        unrecognised("<task/> without id");
        goto error;
    }
    task->task_id = g_strdup((gchar *)task_id);
    xmlFree(task_id);

    gchar *suffix = g_strconcat("tasks/", task->task_id, "/", NULL);
    task->task_uri = soup_uri_new_with_base(recipe_uri, suffix);
    g_free(suffix);

    xmlNode *fetch = first_child_with_name(task_node, "fetch");
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
        task->path = g_build_filename(TASK_LOCATION,
                soup_uri_get_host(task->fetch.url),
                soup_uri_get_path(task->fetch.url),
                soup_uri_get_fragment(task->fetch.url),
                NULL);
    } else {
        task->fetch_method = TASK_FETCH_INSTALL_PACKAGE;
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
        xmlChar *rpm_path = xmlGetNoNsProp(rpm, (xmlChar *)"path");
        if (rpm_path == NULL) {
            unrecognised("Task %s has 'rpm' element without 'path' attribute",
                    task->task_id);
            goto error;
        }
        task->path = g_strdup((gchar *)rpm_path);
        xmlFree(rpm_path);
    }

    xmlNode *params_node = first_child_with_name(task_node, "params");
    if (params_node != NULL) {
        task->params = parse_params(params_node, &tmp_error);
        /* params could be empty, but if parsing causes an error then fail */
        if (tmp_error != NULL) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s has ", task->task_id);
            goto error;
        }
    }

    xmlNode *roles_node = first_child_with_name(task_node, "roles");
    if (roles_node != NULL) {
        task->roles = parse_roles(roles_node, &tmp_error);
        /* roles could be empty, but if parsing causes an error then fail */
        if (tmp_error != NULL) {
            g_propagate_prefixed_error(error, tmp_error,
                    "Task %s has ", task->task_id);
            goto error;
        }
    }

    /*
     * override the run_metadata values for started and finished
     * with the value of status from the xml.
     * When running against the RESTAPI the status value will 
     * update as the recipe executes.
     * When running against a static xml file it won't.
     */
    xmlChar *status = xmlGetNoNsProp(task_node, (xmlChar *)"status");
    if (status == NULL) {
        unrecognised("Task %s missing 'status' attribute", task->task_id);
        goto error;
    }
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
    g_return_val_if_fail(recipe_uri != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GError *tmp_error = NULL;
    gint i = 0;
    Recipe *result = g_slice_new0(Recipe);

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

    result->job_id = get_attribute(recipe, "job_id");
    result->recipe_set_id = get_attribute(recipe, "recipe_set_id");
    result->recipe_id = get_attribute(recipe, "id");
    result->osarch = get_attribute(recipe, "arch");
    result->osdistro = get_attribute(recipe, "distro");
    result->osmajor = get_attribute(recipe, "family");
    result->osvariant = get_attribute(recipe, "variant");
    result->recipe_uri = recipe_uri;

    GList *tasks = NULL;
    xmlNode *child = recipe->children;
    while (child != NULL) {
        if (child->type == XML_ELEMENT_NODE &&
                g_strcmp0((gchar *)child->name, "task") == 0) {
            Task *task = parse_task(child, recipe_uri, &tmp_error);
            if (task == NULL) {
                g_propagate_error(error, tmp_error);
                goto error;
            }
            /* link task to recipe for additional attributes */
            task->recipe = result;

            /*
             * run_path is where we will store the following:
             * taskout.log : STDOUT/STDERR from the task run
             * metadata    : variables like rebootcount and time left on the watchdog
             */
            task->run_path = g_build_filename(TASK_LOCATION, task->recipe->recipe_id, task->task_id, NULL);

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
read_finish (GObject *stream, GAsyncResult *result, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    gssize size;

    size = g_input_stream_read_finish (G_INPUT_STREAM (stream),
                                         result, &app_data->error);

    if (app_data->error || size < 0) {
        g_object_unref(stream);
        app_data->state = RECIPE_COMPLETE;
        return;
    } else if (size == 0) {
        // Finished Reading
        xmlParserErrors result = xmlParseChunk(ctxt, buf, 0, /* terminator */ 1);
        if (result != XML_ERR_OK) {
            xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
            g_set_error_literal(&app_data->error, RESTRAINT_RECIPE_PARSE_ERROR,
                    RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                    xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
            xmlFreeDoc(ctxt->myDoc);
            xmlFreeParserCtxt(ctxt);
            ctxt = NULL;
            g_object_unref(stream);
            /* Set us back to idle so we can accept a valid recipe */
            app_data->state = RECIPE_COMPLETE;
            return;
        }
        g_object_unref(stream);
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
            xmlParserErrors result = xmlParseChunk(ctxt, buf, size,
                    /* terminator */ 0);
            if (result != XML_ERR_OK) {
                xmlError *xmlerr = xmlCtxtGetLastError(ctxt);
                g_set_error_literal(&app_data->error, RESTRAINT_RECIPE_PARSE_ERROR,
                        RESTRAINT_RECIPE_PARSE_ERROR_BAD_SYNTAX,
                        xmlerr != NULL ? xmlerr->message : "Unknown libxml error");
                xmlFreeDoc(ctxt->myDoc);
                xmlFreeParserCtxt(ctxt);
                ctxt = NULL;
                g_object_unref(stream);
                /* Set us back to idle so we can accept a valid recipe */
                app_data->state = RECIPE_COMPLETE;
                return;
            }
        }
        g_input_stream_read_async (G_INPUT_STREAM (stream),
                                   buf,
                                   buf_size,
                                   G_PRIORITY_DEFAULT,
                                   /* cancellable */ NULL,
                                   read_finish, user_data);
    }

}

void
restraint_recipe_parse_xml (GObject *source, GAsyncResult *res, gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    GInputStream *stream;

    stream = soup_request_send_finish (SOUP_REQUEST (source), res, &app_data->error);
    if (!stream) {
        app_data->state = RECIPE_COMPLETE;
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

static gboolean
task_summary (GHashTable *result_states_from, Task *task, GString *s)
{
    gchar *message = NULL;
    gchar *result = g_hash_table_lookup (result_states_from, &task->result_id);
    message = g_strdup_printf ("*  Task: %12s [%-50s] Result: %s Status: %s\n", task->task_id,
                                                                      task->name,
                                                                      result,
                                                                      task->status);
    s = g_string_append(s, message);
    g_free (message);
    if (task->error) {
        message = g_strdup_printf ("* Error: %-80s\n", task->error->message);
        s = g_string_append(s, message);
        g_free (message);
    }
    return task->error != NULL;
}

void
recipe_finish (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    // Report any recipe errors, It is not valid to report any more data
    // to clients once an error has been reported.
    if (app_data->error) {
        connections_error (app_data, app_data->error);
        g_error_free(app_data->error);
        app_data->error = NULL;
    } 
    // close connections to all clients.
    if (app_data->state == RECIPE_IDLE)
        connections_close(app_data, TRUE);
}

gboolean
recipe_handler (gpointer user_data)
{
    AppData *app_data = (AppData *) user_data;
    SoupURI *recipe_uri;
    SoupRequest *request;
    GString *message = g_string_new(NULL);
    gboolean result = TRUE;
    GList *tasks;
    gboolean task_failed = FALSE;

    switch (app_data->state) {
        case RECIPE_FETCH:
            g_string_printf(message, "* Fetching recipe: %s\n", app_data->recipe_url);
            app_data->state = RECIPE_FETCHING;
            request = soup_session_request(soup_session, app_data->recipe_url, &app_data->error);
            // restraint_recipe_parse_xml will move us to the next state
            if (app_data->error) {
                app_data->state = RECIPE_COMPLETE;
            } else {
                soup_request_send_async(request, /* cancellable */ NULL, restraint_recipe_parse_xml, app_data);
            }
            g_object_unref (request);
            break;
        case RECIPE_PARSE:
            g_string_printf(message, "* Parsing recipe\n");
            recipe_uri = soup_uri_new(app_data->recipe_url);
            app_data->recipe = recipe_parse(ctxt->myDoc, recipe_uri, &app_data->error);
            xmlFreeParserCtxt(ctxt);
            xmlFreeDoc(ctxt->myDoc);
            ctxt = NULL;
            if (app_data->recipe && ! app_data->error) {
                app_data->tasks = app_data->recipe->tasks;
                app_data->state = RECIPE_RUN;
            } else {
                app_data->state = RECIPE_COMPLETE;
            }
            break;
        case RECIPE_RUN:
            app_data->task_handler_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                        task_handler,
                                                        app_data,
                                                        NULL);
            g_string_printf(message, "* Running recipe\n");
            app_data->state = RECIPE_RUNNING;
            break;
        case RECIPE_RUNNING:
            // close connections to all clients that didn't request monitoring
            connections_close(app_data, FALSE);
            return FALSE;
            break;
        case RECIPE_COMPLETE:
            // Generate Summary report
            if (app_data->recipe) {
              tasks = app_data->recipe->tasks;
              message = g_string_append(message, "\n* Results Summary\n");
              do {
                  task_failed |= task_summary (app_data->result_states_from, tasks->data, message);
              } while ((tasks = g_list_next (tasks)) != NULL);
              restraint_recipe_free(app_data->recipe);
              app_data->recipe = NULL;
            }

            // If app_data->error hasn't already been set and one of the tasks
            // failed then propagate that error to the clients.
            if (!app_data->error && task_failed) {
                g_set_error (&app_data->error, RESTRAINT_TASK_RUNNER_ERROR,
                             RESTRAINT_TASK_RUNNER_RESULT_ERROR,
                             "One or more tasks failed");
            }

            // We are done. remove ourselves so we can run another recipe.
            app_data->state = RECIPE_IDLE;
            result = FALSE;
            break;
        default:
            break;
    }

    // write message out to all clients
    if (message->len) {
      connections_write(app_data, message, STREAM_STDERR, 0);
      g_string_free(message, TRUE);
    }

    return result;
}
