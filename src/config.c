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

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "errors.h"
#include "config.h"

#define is_key_file_not_found_error(e) (G_KEY_FILE_ERROR_KEY_NOT_FOUND == e->code \
                                        || G_KEY_FILE_ERROR_GROUP_NOT_FOUND == e->code)

/*
 * Returns the content of file into a new GKeyFile structure.
 *
 * If the file doesn't exist, an empty GKeyFile is returned.
 *
 * If the file cannot be loaded, NULL is returned and err is set.
 */
static GKeyFile *
restraint_config_read_key_file (const gchar  *file,
                                GError      **err)
{
    GKeyFile *key_file;
    GKeyFileFlags flags;
    GError *tmp_err = NULL;

    key_file = g_key_file_new ();

    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    if (!g_key_file_load_from_file (key_file, file, flags, &tmp_err)) {
        if (!g_error_matches (tmp_err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            goto error;

        g_clear_error (&tmp_err);
    }

    return key_file;

  error:
    g_propagate_error (err, tmp_err);
    g_key_file_free (key_file);

    return NULL;
}

gint64
restraint_config_get_int64 (gchar *config_file, gchar *section, gchar *key, GError **error)
{
    g_return_val_if_fail(config_file != NULL, -1);
    g_return_val_if_fail(section != NULL, -1);
    g_return_val_if_fail(error == NULL || *error == NULL, -1);

    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    gint64 value = g_key_file_get_int64 (keyfile,
                                         section,
                                         key,
                                         &tmp_error);

    if (NULL != tmp_error) {
        if (!is_key_file_not_found_error (tmp_error))
            goto error;

        g_clear_error (&tmp_error);
    }

    return value;

  error:
    g_propagate_prefixed_error (error, tmp_error, "config get int64,");

    return 0;
}

guint64
restraint_config_get_uint64 (gchar *config_file, gchar *section, gchar *key, GError **error)
{
    g_return_val_if_fail(config_file != NULL, -1);
    g_return_val_if_fail(section != NULL, -1);
    g_return_val_if_fail(error == NULL || *error == NULL, -1);

    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    guint64 value = g_key_file_get_uint64 (keyfile,
                                           section,
                                           key,
                                           &tmp_error);
    if (NULL != tmp_error) {
        if (!is_key_file_not_found_error (tmp_error))
            goto error;

        g_clear_error (&tmp_error);
    }

    return value;

  error:
    g_propagate_prefixed_error (error, tmp_error, "config get uint64,");

    return 0;
}

gboolean
restraint_config_get_boolean (gchar *config_file, gchar *section, gchar *key, GError **error)
{
    g_return_val_if_fail(config_file != NULL, FALSE);
    g_return_val_if_fail(section != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;
    gboolean value;

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    value = g_key_file_get_boolean (keyfile, section, key, &tmp_error);

    if (NULL != tmp_error) {
        if (!is_key_file_not_found_error (tmp_error))
            goto error;

        g_clear_error(&tmp_error);
    }

    return value;

  error:
    g_propagate_prefixed_error (error, tmp_error, "config get gboolean,");

    return FALSE;
}

gchar *
restraint_config_get_string (gchar *config_file, gchar *section, gchar *key, GError **error)
{
    g_return_val_if_fail(config_file != NULL, NULL);
    g_return_val_if_fail(section != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    gchar *value = g_key_file_get_string (keyfile,
                                          section,
                                          key,
                                          &tmp_error);
    if (NULL != tmp_error) {
        if (!is_key_file_not_found_error (tmp_error))
            goto error;

        g_clear_error(&tmp_error);
    }

    return value;

  error:
    g_propagate_prefixed_error (error, tmp_error, "config get string,");

    return NULL;
}

gchar **
restraint_config_get_keys (gchar *config_file, gchar *section, GError **error)
{
    g_return_val_if_fail(config_file != NULL, NULL);
    g_return_val_if_fail(section != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;
    gchar **ret = NULL;

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    ret = g_key_file_get_keys(keyfile, section, NULL, &tmp_error);

    if (NULL != tmp_error) {
        if (!is_key_file_not_found_error (tmp_error))
            goto error;

        g_clear_error(&tmp_error);
    }

    return ret;

  error:
    g_propagate_prefixed_error(error, tmp_error, "config get keys,");

    return NULL;
}

static gint
restraint_mkdir_parent (const gchar *file)
{
    g_autofree gchar *dirname = NULL;

    dirname = g_path_get_dirname (file);

    return g_mkdir_with_parents (dirname, 0755 /* drwxr-xr-x */);
}

void
restraint_config_trunc (gchar *config_file, GError **error)
{
    g_return_if_fail(config_file != NULL);

    GError *tmp_error = NULL;

    restraint_mkdir_parent (config_file);

    if (!g_file_set_contents (config_file, "", -1,  &tmp_error)) {
        g_propagate_error (error, tmp_error);
    }
}

void
restraint_config_set (gchar *config_file, const gchar *section,
                      const gchar *key, GError **gerror, GType type, ...)
{
    g_return_if_fail(config_file != NULL);
    g_return_if_fail(section != NULL);
    g_return_if_fail(gerror == NULL || *gerror == NULL);
    va_list args;
    GValue value;

    g_autofree gchar *s_data = NULL;
    gsize length;
    g_autoptr (GKeyFile) keyfile = NULL;
    GError *tmp_error = NULL;

    restraint_mkdir_parent (config_file);

    keyfile = restraint_config_read_key_file (config_file, &tmp_error);

    if (NULL != tmp_error)
        goto error;

    if (key && type != -1) {
        va_start (args, type);
        SOUP_VALUE_SETV (&value, type, args);
        va_end (args);
        switch (type) {
            case G_TYPE_UINT64:
                g_key_file_set_uint64 (keyfile,
                                       section,
                                       key,
                                       g_value_get_uint64 (&value));
                break;
            case G_TYPE_INT:
                g_key_file_set_integer (keyfile,
                                       section,
                                       key,
                                       g_value_get_int (&value));
                break;
            case G_TYPE_BOOLEAN:
                g_key_file_set_boolean (keyfile,
                                       section,
                                       key,
                                       g_value_get_boolean (&value));
                break;
            case G_TYPE_STRING:
                g_key_file_set_string (keyfile,
                                       section,
                                       key,
                                       g_value_get_string (&value));
                break;
            default:
                g_critical ("%s (): invalid GType", __func__);

                return;
        }
    } else if (key) {
        // no value, remove the key
        g_key_file_remove_key (keyfile,
                               section,
                               key,
                               NULL);
    } else {
        // key and value are NULL, remove the whole group
        g_key_file_remove_group (keyfile,
                                 section,
                                 NULL);
    }

    /* g_key_file_to_data never reports errors, and always returns a NULL
       terminated string. */
    s_data = g_key_file_to_data (keyfile, &length, NULL);

    if (!g_file_set_contents (config_file, s_data, length,  &tmp_error))
        goto error;

    return;

  error:
    g_propagate_error (gerror, tmp_error);
}
