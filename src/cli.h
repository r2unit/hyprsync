#ifndef HS_CLI_H
#define HS_CLI_H

#include "config.h"

typedef struct {
    char *command;
    char *config_path;
    int dry_run;
    int verbose;
    int quiet;
    int devel;
    char *group;
    char *device;
    hs_strvec args;
} hs_cli_options;

typedef struct {
    hs_cli_options options;
    hs_config config;
    int config_loaded;
} hs_cli;

void hs_cli_init(hs_cli *cli, int argc, char *argv[]);
int hs_cli_run(hs_cli *cli);
void hs_cli_free(hs_cli *cli);

#endif
