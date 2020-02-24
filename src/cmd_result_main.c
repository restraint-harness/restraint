#include "cmd_result.h"

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;
    AppData *app_data = restraint_create_appdata();

    if(!parse_arguments(app_data, argc, argv)){
        rc = EXIT_FAILURE;
        goto cleanup;
    }

    if(!upload_results(app_data)){
        rc = EXIT_FAILURE;
        goto cleanup;
    }

    rc = EXIT_SUCCESS;
cleanup:
    restraint_free_appdata(app_data);
    return rc;
}
