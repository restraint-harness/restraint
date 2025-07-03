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
#include <libsoup/soup.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <stdio.h>
#include "errors.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

gboolean
upload_file_curl (gchar *filepath,
                  gchar *filename,
                  const gchar *upload_url,
                  GError **error)
{
    CURL *curl;
    CURLcode res;
    FILE *file;
    struct stat file_info;
    long response_code;
    gboolean result = TRUE;
    
    /* Get file size */
    if (stat(filepath, &file_info) != 0) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Error getting file info: %s", filepath);
        return FALSE;
    }
    
    /* Open file for reading */
    file = fopen(filepath, "rb");
    if (!file) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Error opening file: %s", filepath);
        return FALSE;
    }
    
    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to initialize curl");
        fclose(file);
        return FALSE;
    }
    
    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, upload_url);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "restraint-client");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    
    /* Set content type header */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/plain");
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    /* Perform the upload */
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Failed to upload file: %s", curl_easy_strerror(res));
        result = FALSE;
        goto cleanup;
    }
    
    /* Check HTTP response code */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code < 200 || response_code >= 300) {
        g_set_error (error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Upload failed with HTTP status: %ld", response_code);
        result = FALSE;
    }
    
cleanup:
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    fclose(file);
    
    return result;
}
