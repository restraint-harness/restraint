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
#include <curl/curl.h>

#include "upload.h"
#include "errors.h"

gchar *tmp_test_dir = NULL;

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
    const gchar         *upload_url = "http://localhost:8000/";

    log_path = g_build_filename (tmp_test_dir, "dummy.log", NULL);

    log_size = (128 * 2.5) * 1024;

    mk_dummy_log (log_path, log_size);

    success = upload_file_curl (log_path, "dummy.log", upload_url, &err);

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
    const gchar         *upload_url = "https://thehostthatdoesntexist:8123/";

    log_path = g_build_filename (tmp_test_dir, "dummy.log", NULL);

    mk_dummy_log (log_path, 1);

    success = upload_file_curl (log_path, "dummy.log", upload_url, &err);

    g_assert_error (err, RESTRAINT_ERROR, RESTRAINT_PARSE_ERROR_BAD_SYNTAX);
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
    const gchar        *upload_url = "http://localhost:8000/";

    success = upload_file_curl ("the/file/that/doesnt/exist", "neexistuje", upload_url, &err);

    g_assert_error (err, RESTRAINT_ERROR, RESTRAINT_PARSE_ERROR_BAD_SYNTAX);
    g_assert_false (success);
}

int
main (int    argc,
      char **argv)
{
    int retval;

    tmp_test_dir = g_dir_make_tmp ("test_upload_XXXXXX", NULL);

    /* Initialize curl globally */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/upload/upload_file/dummy_log", test_upload_file_dummy_log);
    g_test_add_func ("/upload/upload_file/no_file", test_upload_file_no_file);
    g_test_add_func ("/upload/upload_file/bad_host", test_upload_file_bad_host);

    retval = g_test_run ();

    /* Cleanup curl */
    curl_global_cleanup();
    
    g_remove (tmp_test_dir);
    g_free (tmp_test_dir);

    return retval;
}
