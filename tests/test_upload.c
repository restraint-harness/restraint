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
#include <glib/gstdio.h>
#include <libsoup/soup.h>

#include "upload.h"

gchar *tmp_test_dir = NULL;
SoupSession *soup_session = NULL;

static void
mk_dummy_log (const gchar *path,
              gsize size)
{
    FILE *file = NULL;

    file = g_fopen (path, "w");

    g_assert_nonnull (file);

    for (guint line = 0; size > 0; line++) {
        gint bytes;

        bytes = g_fprintf (file, "%8d: All test and no code makes Jack a dull boy\n", line);

        g_assert_cmpint (bytes, >, 0);

        size = bytes >= size ? 0 : (size - bytes);
    }

    fclose (file);
}

static void
test_upload_file_dummy_log (void)
{
    gboolean             success;
    gsize                log_size;
    g_autofree gchar    *log_path = NULL;
    g_autoptr (GError)   err = NULL;
    g_autoptr (SoupURI)  uri = NULL;

    log_path = g_build_filename (tmp_test_dir, "dummy.log", NULL);

    log_size = (128 * 2.5) * 1024;

    mk_dummy_log (log_path, log_size);

    uri = soup_uri_new ("http://localhost:8000/");

    success = upload_file (soup_session, log_path, "dummy.log", uri, &err);

    g_assert_no_error (err);
    g_assert_true (success);

    g_remove (log_path);
}

/*
 * Tests code path for handling http request errors.
 */
static void
test_upload_file_bad_host (void)
{
    gboolean             success;
    g_autofree gchar    *log_path = NULL;
    g_autoptr (GError)   err = NULL;
    g_autoptr (SoupURI)  uri = NULL;

    log_path = g_build_filename (tmp_test_dir, "dummy.log", NULL);

    mk_dummy_log (log_path, 1);

    uri = soup_uri_new ("https://thehostthatdoesntexist:8123/");

    success = upload_file (soup_session, log_path, "dummy.log", uri, &err);

    g_assert_error (err, SOUP_HTTP_ERROR, SOUP_STATUS_CANT_RESOLVE);
    g_assert_false (success);

    g_remove (log_path);
}

/*
 * Tests code path for handling file related errors.
 */
static void
test_upload_file_no_file (void)
{
    gboolean            success;
    g_autoptr (GError)  err = NULL;
    g_autoptr (SoupURI) uri = NULL;

    uri = soup_uri_new ("http://localhost:8000/");

    success = upload_file (soup_session, "the/file/that/doesnt/exist", "neexistuje", uri, &err);

    g_assert_error (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_assert_false (success);
}

int
main (int    argc,
      char **argv)
{
    int retval;

    tmp_test_dir = g_dir_make_tmp ("test_upload_XXXXXX", NULL);
    soup_session = soup_session_new ();

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/upload/upload_file/dummy_log", test_upload_file_dummy_log);
    g_test_add_func ("/upload/upload_file/no_file", test_upload_file_no_file);
    g_test_add_func ("/upload/upload_file/bad_host", test_upload_file_bad_host);

    retval = g_test_run ();

    g_object_unref (soup_session);
    g_remove (tmp_test_dir);
    g_free (tmp_test_dir);

    return retval;
}
