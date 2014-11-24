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

#include "metadata.h"

static void test_testinfo_dependencies(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);
    gchar *dependency;

    gchar *filename = "test-data/parse_testinfo/dependencies/requires/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->dependencies, 0);
    g_assert_cmpstr(dependency, ==, "gcc");
    dependency = g_slist_nth_data(metadata->dependencies, 1);
    g_assert_cmpstr(dependency, ==, "bzip2");
    dependency = g_slist_nth_data(metadata->dependencies, 2);
    g_assert_cmpstr(dependency, ==, "rusers");
    dependency = g_slist_nth_data(metadata->dependencies, 10);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_slist_nth_data(metadata->dependencies, 11);
    g_assert_cmpstr(dependency, ==, "lib-virt");

    restraint_metadata_free (metadata);
}

static void test_testinfo_repodeps(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);
    gchar *dependency;

    gchar *filename = "test-data/parse_testinfo/dependencies/requires/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->repodeps, 0);
    g_assert_cmpstr(dependency, ==, "/kernel/common");
    dependency = g_slist_nth_data(metadata->repodeps, 1);
    g_assert_cmpstr(dependency, ==, "/kernel/include");

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_day(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_testinfo/testtime/day/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 24 * 2);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_hour(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_testinfo/testtime/hour/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 10);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_invalid(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_testinfo/testtime/invalid/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_cmpstr(error->message, ==, "Unrecognised time unit 'G'");
    g_clear_error(&error);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_minute(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_testinfo/testtime/minute/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 20);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_second(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_testinfo/testtime/second/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 200);

    restraint_metadata_free (metadata);
}

static void test_metadata_dependencies(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/dependencies/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";
    gchar *dependency;

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->dependencies, 0);
    g_assert_cmpstr(dependency, ==, "gcc");
    dependency = g_slist_nth_data(metadata->dependencies, 1);
    g_assert_cmpstr(dependency, ==, "bzip2");
    dependency = g_slist_nth_data(metadata->dependencies, 2);
    g_assert_cmpstr(dependency, ==, "rusers");
    dependency = g_slist_nth_data(metadata->dependencies, 10);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_slist_nth_data(metadata->dependencies, 11);
    g_assert_cmpstr(dependency, ==, "lib-virt");

    filename = "test-data/parse_metadata/dependencies/metadata-no-deps";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error (error);

    restraint_metadata_free (metadata);
}

static void test_metadata_repodeps(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/dependencies/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";
    gchar *dependency;

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->repodeps, 0);
    g_assert_cmpstr(dependency, ==, "kernel/include");
    dependency = g_slist_nth_data(metadata->repodeps, 1);
    g_assert_cmpstr(dependency, ==, "restraint/sanity/common");

    filename = "test-data/parse_metadata/dependencies/metadata-no-deps";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error (error);

    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_day(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/testtime/day/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 24 * 2);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_hour(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/testtime/hour/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 10);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_invalid(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/testtime/invalid/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_cmpstr(error->message, ==, "Unrecognised time unit 'G'");
    g_error_free(error);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_minute(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/testtime/minute/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 20);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_second(void) {
    GError *error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    gchar *filename = "test-data/parse_metadata/testtime/second/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 200);
    restraint_metadata_free (metadata);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/testindo.desc/testtime/day", test_testinfo_testtime_day);
    g_test_add_func("/testindo.desc/testtime/hour", test_testinfo_testtime_hour);
    g_test_add_func("/testindo.desc/testtime/invalid", test_testinfo_testtime_invalid);
    g_test_add_func("/testindo.desc/testtime/minute", test_testinfo_testtime_minute);
    g_test_add_func("/testindo.desc/testtime/second", test_testinfo_testtime_second);
    g_test_add_func("/testinfo.desc/dependencies", test_testinfo_dependencies);
    g_test_add_func("/testinfo.desc/repodeps", test_testinfo_repodeps);
    g_test_add_func("/metadata/testtime/day", test_metadata_testtime_day);
    g_test_add_func("/metadata/testtime/hour", test_metadata_testtime_hour);
    g_test_add_func("/metadata/testtime/invalid", test_metadata_testtime_invalid);
    g_test_add_func("/metadata/testtime/minute", test_metadata_testtime_minute);
    g_test_add_func("/metadata/testtime/second", test_metadata_testtime_second);
    g_test_add_func("/metadata/dependencies", test_metadata_dependencies);
    g_test_add_func("/metadata/repodeps", test_metadata_repodeps);
    return g_test_run();
}
