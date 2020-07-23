#include <stdlib.h>
#include <glib.h>
#include <libsoup/soup.h>

#include "beaker_harness.h"

int mocked_soup_status = -1;

guint __real_soup_session_send_message (SoupSession *session, SoupMessage *msg);

guint
__wrap_soup_session_send_message (SoupSession *session,
                                  SoupMessage *msg)
{
    if (mocked_soup_status < 0)
        return __real_soup_session_send_message (session, msg);

    soup_message_set_status (msg, mocked_soup_status);

    if (mocked_soup_status == SOUP_STATUS_OK) {
        soup_message_headers_append (msg->response_headers, "Content-Type", "application/xml");
        soup_message_headers_set_content_length (msg->response_headers, 42);
    }

    return mocked_soup_status;
}

static void
test_rstrnt_bkr_check_recipe_no_lc (gconstpointer test_data)
{
    const char *recipe_url;

    recipe_url = (char *) test_data;

    mocked_soup_status = -1;

    g_assert_cmpint (BKR_HEALTH_STATUS_UNKNOWN, ==, rstrnt_bkr_check_recipe (recipe_url));
}

static void
test_rstrnt_bkr_check_recipe_good (gconstpointer test_data)
{
    bkr_health_status_t status;
    const char *recipe_url;

    recipe_url = (char *) test_data;

    mocked_soup_status = SOUP_STATUS_OK;

    status = rstrnt_bkr_check_recipe (recipe_url);

    mocked_soup_status = -1;

    g_assert_cmpint (BKR_HEALTH_STATUS_GOOD, ==, status);
}

static void
test_rstrnt_bkr_check_recipe_bad (gconstpointer test_data)
{
    bkr_health_status_t status;
    const char *recipe_url;

    recipe_url = (char *) test_data;

    mocked_soup_status = SOUP_STATUS_NOT_FOUND;

    status = rstrnt_bkr_check_recipe (recipe_url);

    mocked_soup_status = -1;

    g_assert_cmpint (BKR_HEALTH_STATUS_BAD, ==, status);
}

int
main (int   argc,
      char *argv[])
{
    const char * recipe_url;

    recipe_url = "http://nohost:4242/recipes/1/";

    g_test_init (&argc, &argv, NULL);

    g_test_add_data_func ("/beaker/harness/health/bad", recipe_url, test_rstrnt_bkr_check_recipe_bad);
    g_test_add_data_func ("/beaker/harness/health/good", recipe_url, test_rstrnt_bkr_check_recipe_good);
    g_test_add_data_func ("/beaker/harness/health/no_lc", recipe_url, test_rstrnt_bkr_check_recipe_no_lc);

    return g_test_run ();
}
