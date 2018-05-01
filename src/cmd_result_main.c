#include "cmd_result.h"

int main(int argc, char *argv[])
{
    int rc = -1;
    AppData *app_data = restraint_create_appdata();

    if(!parse_arguments(app_data, argc, argv)){
        rc = -1;
        goto cleanup;
    }

    if(!upload_results(app_data)){
        rc = -2;
        goto cleanup;
    }

    rc = 0;
cleanup:
    restraint_free_appdata(app_data);
    return rc;
}
