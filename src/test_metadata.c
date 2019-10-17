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
#include "param.h"

/* Helper function for the environment metadata tests */
static void check_metadata_envvars(GSList *meta_list) {
    int i = 0;

    /* Should match the 'environment' in the metadata
       This needs to be the reverse order of metadata due to list prepending */
    static Param params[] = {
        {"VARWITHANEQUALS", "THIS=THAT"},
        {"MYCOOLVARIABLENAME", "MYCOOLVARIABLEVALUE"},
        {"FOO", "BAR"},
    };
    const int param_size = sizeof(params) / sizeof(Param);

    for(
        ;
        meta_list != NULL;
        meta_list = meta_list->next
    ){
        g_assert_cmpint(i, <, param_size);
        Param *p = (Param*) meta_list->data;
        g_assert_cmpstr(params[i].name, ==, p->name);
        g_assert_cmpstr(params[i].value, ==, p->value);
        i++;
    }
}

static void test_testinfo_dependencies(void) {
    GError *error = NULL;
    MetaData *metadata;
    gchar *dependency;

    gchar *filename = "test-data/parse_testinfo/dependencies/requires/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->dependencies, 0);
    g_assert_cmpstr(dependency, ==, "test(/and/this/too)");
    dependency = g_slist_nth_data(metadata->dependencies, 1);
    g_assert_cmpstr(dependency, ==, "test(/this/should/not/be/ignored)");
    dependency = g_slist_nth_data(metadata->dependencies, 2);
    g_assert_cmpstr(dependency, ==, "gcc");
    dependency = g_slist_nth_data(metadata->dependencies, 12);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_slist_nth_data(metadata->dependencies, 13);
    g_assert_cmpstr(dependency, ==, "lib-virt");

    restraint_metadata_free (metadata);
}

static void test_testinfo_repodeps(void) {
    GError *error = NULL;
    MetaData *metadata;
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

static void test_testinfo_use_pty(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/use_pty/true/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_true (metadata->use_pty);

    restraint_metadata_free (metadata);

    filename = "test-data/parse_testinfo/use_pty/false/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_false (metadata->use_pty);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_day(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/testtime/day/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 24 * 2);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_hour(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/testtime/hour/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 10);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_invalid(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/testtime/invalid/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_cmpstr(error->message, ==, "Unrecognised time unit 'G'");
    g_clear_error(&error);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_minute(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/testtime/minute/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 20);

    restraint_metadata_free (metadata);
}

static void test_testinfo_testtime_second(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/testtime/second/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 200);

    restraint_metadata_free (metadata);
}

static void test_testinfo_environment(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/environment/testinfo.desc";

    metadata = restraint_parse_testinfo (filename, &error);

    g_assert_no_error (error);

    check_metadata_envvars(metadata->envvars);

    restraint_metadata_free (metadata);
}

static void test_testinfo_other(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_testinfo/other/testinfo.desc";

    metadata = restraint_parse_testinfo(filename, &error);

    g_assert_no_error(error);
    g_assert_cmpstr(metadata->name, ==, "/TOPLEVEL/filesystems/nfs/testsuite");

    restraint_metadata_free (metadata);
}

static void check_dependencies(GSList *dependencies) {
    gchar *dependency;

    dependency = g_slist_nth_data(dependencies, 0);
    g_assert_cmpstr(dependency, ==, "gcc");
    dependency = g_slist_nth_data(dependencies, 1);
    g_assert_cmpstr(dependency, ==, "bzip2");
    dependency = g_slist_nth_data(dependencies, 2);
    g_assert_cmpstr(dependency, ==, "rusers");
    dependency = g_slist_nth_data(dependencies, 10);
    g_assert_cmpstr(dependency, ==, "httpd");
    dependency = g_slist_nth_data(dependencies, 11);
    g_assert_cmpstr(dependency, ==, "lib-virt");

}

static void test_metadata_dependencies(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/dependencies/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error(error);
    check_dependencies(metadata->dependencies);
    g_assert(metadata->softdependencies==NULL);
    restraint_metadata_free(metadata);

    filename = "test-data/parse_metadata/dependencies/metadata-soft";
    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error(error);
    check_dependencies(metadata->softdependencies);
    g_assert(metadata->dependencies==NULL);
    restraint_metadata_free(metadata);

    filename = "test-data/parse_metadata/dependencies/metadata-no-deps";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error(error);

    restraint_metadata_free(metadata);
}

static void test_metadata_repodeps(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/dependencies/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";
    gchar *dependency;

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    dependency = g_slist_nth_data(metadata->repodeps, 0);
    g_assert_cmpstr(dependency, ==, "kernel/include");
    dependency = g_slist_nth_data(metadata->repodeps, 1);
    g_assert_cmpstr(dependency, ==, "restraint/sanity/common");

    restraint_metadata_free (metadata);

    filename = "test-data/parse_metadata/dependencies/metadata-no-deps";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error (error);

    restraint_metadata_free(metadata);
}

static void test_metadata_use_pty(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/use_pty/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_true (metadata->use_pty);

    restraint_metadata_free (metadata);

    filename = "test-data/parse_metadata/use_pty/metadata-no-use_pty";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error (error);
    g_assert_false (metadata->use_pty);

    restraint_metadata_free (metadata);
}

static void test_metadata_no_localwatchdog(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/no_localwatchdog/no_localwatchdog";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error(error);
    g_assert_true(metadata->nolocalwatchdog);

    restraint_metadata_free(metadata);

    filename = "test-data/parse_metadata/no_localwatchdog/localwatchdog_on";
    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error(error);
    g_assert_false(metadata->nolocalwatchdog);

    restraint_metadata_free(metadata);

    filename = "test-data/parse_metadata/no_localwatchdog/no_localwatchdog_dflt";
    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error(error);
    g_assert_false(metadata->use_pty);

    restraint_metadata_free(metadata);
}

static void test_metadata_other(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/other/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error(error);
    g_assert_cmpstr(metadata->name, ==, "/restraint/sanity/fetch_git");
    g_assert_cmpstr(metadata->entry_point, ==, "make run");

    restraint_metadata_free(metadata);

}

static void test_metadata_testtime_day(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/testtime/day/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 24 * 2);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_hour(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/testtime/hour/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 60 * 10);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_invalid(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/testtime/invalid/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_cmpstr(error->message, ==, "Unrecognised time unit 'G'");
    g_error_free(error);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_minute(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/testtime/minute/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 60 * 20);
    restraint_metadata_free (metadata);
}

static void test_metadata_testtime_second(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/testtime/second/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);

    g_assert_no_error (error);
    g_assert_cmpuint(metadata->max_time, ==, 200);
    restraint_metadata_free (metadata);
}

static void test_metadata_environment(void) {
    GError *error = NULL;
    MetaData *metadata;

    gchar *filename = "test-data/parse_metadata/environment/metadata";
    gchar *osmajor = "RedHatEnterpriseLinux6";

    metadata = restraint_parse_metadata(filename, osmajor, &error);
    g_assert_no_error (error);
    g_assert_nonnull (metadata->envvars);

    check_metadata_envvars(metadata->envvars);

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
    g_test_add_func("/testinfo.desc/use_pty", test_testinfo_use_pty);
    g_test_add_func("/testinfo.desc/environment", test_testinfo_environment);
    g_test_add_func("/testinfo.desc/other", test_testinfo_other);
    g_test_add_func("/metadata/testtime/day", test_metadata_testtime_day);
    g_test_add_func("/metadata/testtime/hour", test_metadata_testtime_hour);
    g_test_add_func("/metadata/testtime/invalid", test_metadata_testtime_invalid);
    g_test_add_func("/metadata/testtime/minute", test_metadata_testtime_minute);
    g_test_add_func("/metadata/testtime/second", test_metadata_testtime_second);
    g_test_add_func("/metadata/dependencies", test_metadata_dependencies);
    g_test_add_func("/metadata/repodeps", test_metadata_repodeps);
    g_test_add_func("/metadata/other", test_metadata_other);
    g_test_add_func("/metadata/use_pty", test_metadata_use_pty);
    g_test_add_func("/metadata/no_localwatchdog", test_metadata_no_localwatchdog);
    g_test_add_func("/metadata/environment", test_metadata_environment);
    return g_test_run();
}
