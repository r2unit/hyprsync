#include "cli.h"
#include "log.h"

int main(int argc, char *argv[]) {
    hs_log_set_level(HS_LOG_INFO);

    hs_cli cli;
    hs_cli_init(&cli, argc, argv);
    int rc = hs_cli_run(&cli);
    hs_cli_free(&cli);
    return rc;
}
