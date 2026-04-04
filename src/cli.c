#include "cli.h"
#include "config.h"
#include "daemon.h"
#include "git.h"
#include "log.h"
#include "setup.h"
#include "sync.h"
#include "tui.h"
#include "upgrade.h"
#include "util.h"

#include <hyprsync/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void);
static int load_config(hs_cli *cli);
static int cmd_init(hs_cli *cli);
static int cmd_daemon(hs_cli *cli);
static int cmd_sync(hs_cli *cli);
static int cmd_status(hs_cli *cli);
static int cmd_diff(hs_cli *cli);
static int cmd_log(hs_cli *cli);
static int cmd_ping(hs_cli *cli);
static int cmd_conflicts(hs_cli *cli);
static int cmd_restore(hs_cli *cli);
static int cmd_upgrade(hs_cli *cli);
static int cmd_version(hs_cli *cli);
static int cmd_help(hs_cli *cli);

void hs_cli_init(hs_cli *cli, int argc, char *argv[]) {
    memset(cli, 0, sizeof(*cli));
    hs_vec_init(&cli->options.args);
    cli->config_loaded = 0;

    cli->options.config_path = hs_default_config_path();
    cli->options.command = NULL;
    cli->options.group = NULL;
    cli->options.device = NULL;
    cli->options.dry_run = 0;
    cli->options.verbose = 0;
    cli->options.quiet = 0;
    cli->options.devel = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-c") == 0 || strcmp(arg, "--config") == 0) {
            if (i + 1 < argc) {
                free(cli->options.config_path);
                cli->options.config_path = hs_expand_path(argv[++i]);
            }
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--dry-run") == 0) {
            cli->options.dry_run = 1;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            cli->options.verbose = 1;
        } else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
            cli->options.quiet = 1;
        } else if (strcmp(arg, "--devel") == 0) {
            cli->options.devel = 1;
        } else if (strcmp(arg, "-g") == 0 || strcmp(arg, "--group") == 0) {
            if (i + 1 < argc) {
                free(cli->options.group);
                cli->options.group = strdup(argv[++i]);
            }
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--device") == 0) {
            if (i + 1 < argc) {
                free(cli->options.device);
                cli->options.device = strdup(argv[++i]);
            }
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            free(cli->options.command);
            cli->options.command = strdup("help");
        } else if (arg[0] != '-') {
            if (!cli->options.command) {
                cli->options.command = strdup(arg);
            } else {
                hs_vec_push(&cli->options.args, strdup(arg));
            }
        }
    }

    if (!cli->options.command) {
        cli->options.command = strdup("help");
    }
}

static int load_config(hs_cli *cli) {
    if (cli->config_loaded) {
        return 0;
    }

    cli->config = hs_load_config(cli->options.config_path);

    if (!cli->config.hostname) {
        hs_debug("could not load config from %s", cli->options.config_path);
        return -1;
    }

    cli->config_loaded = 1;

    if (cli->options.dry_run) {
        cli->config.dry_run = 1;
    }

    if (cli->options.verbose) {
        hs_log_set_level(HS_LOG_DEBUG);
    } else if (cli->options.quiet) {
        hs_log_set_level(HS_LOG_ERROR);
    } else {
        hs_log_set_level(hs_log_level_from_string(cli->config.log_level));
    }

    return 0;
}

int hs_cli_run(hs_cli *cli) {
    const char *cmd = cli->options.command;

    if (strcmp(cmd, "init") == 0) return cmd_init(cli);
    if (strcmp(cmd, "daemon") == 0) return cmd_daemon(cli);
    if (strcmp(cmd, "sync") == 0) return cmd_sync(cli);
    if (strcmp(cmd, "status") == 0) return cmd_status(cli);
    if (strcmp(cmd, "diff") == 0) return cmd_diff(cli);
    if (strcmp(cmd, "log") == 0) return cmd_log(cli);
    if (strcmp(cmd, "ping") == 0) return cmd_ping(cli);
    if (strcmp(cmd, "conflicts") == 0) return cmd_conflicts(cli);
    if (strcmp(cmd, "restore") == 0) return cmd_restore(cli);
    if (strcmp(cmd, "upgrade") == 0) return cmd_upgrade(cli);
    if (strcmp(cmd, "version") == 0) return cmd_version(cli);
    if (strcmp(cmd, "help") == 0) return cmd_help(cli);

    fprintf(stderr, "unknown command: %s\n", cmd);
    print_usage();
    return 1;
}

void hs_cli_free(hs_cli *cli) {
    free(cli->options.command);
    free(cli->options.config_path);
    free(cli->options.group);
    free(cli->options.device);
    hs_strvec_free(&cli->options.args);

    if (cli->config_loaded) {
        hs_config_free(&cli->config);
    }
}

static int cmd_init(hs_cli *cli) {
    (void)cli;
    hs_config cfg = hs_setup_run();

    if (!cfg.hostname) {
        fprintf(stderr, "setup failed\n");
        return 1;
    }

    hs_config_free(&cfg);
    return 0;
}

static int cmd_daemon(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_daemon *d = hs_daemon_create(cli->config);
    hs_daemon_run(d);
    hs_daemon_free(d);
    return 0;
}

static int cmd_sync(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized. run 'hyprsync init' first.\n");
        hs_git_free(git);
        return 1;
    }

    hs_sync *s = hs_sync_create(&cli->config, git);
    int dry_run = cli->options.dry_run || cli->config.dry_run;

    if (dry_run) {
        printf("dry-run mode: showing what would be synced\n\n");
    }

    if (cli->options.group && cli->options.device) {
        const hs_sync_group *group = NULL;
        const hs_device *device = NULL;

        for (size_t i = 0; i < cli->config.sync_groups.len; i++) {
            if (strcmp(cli->config.sync_groups.data[i].name, cli->options.group) == 0) {
                group = &cli->config.sync_groups.data[i];
                break;
            }
        }

        for (size_t i = 0; i < cli->config.devices.len; i++) {
            if (strcmp(cli->config.devices.data[i].name, cli->options.device) == 0) {
                device = &cli->config.devices.data[i];
                break;
            }
        }

        if (!group) {
            fprintf(stderr, "unknown group: %s\n", cli->options.group);
            hs_sync_free(s);
            hs_git_free(git);
            return 1;
        }

        if (!device) {
            fprintf(stderr, "unknown device: %s\n", cli->options.device);
            hs_sync_free(s);
            hs_git_free(git);
            return 1;
        }

        hs_sync_result result = hs_sync_run_group(s, group, device, dry_run);

        if (result.success) {
            printf("synced %s to %s\n", result.group_name, result.device_name);
        } else {
            fprintf(stderr, "failed to sync %s to %s: %s\n",
                    result.group_name, result.device_name, result.error_message);
            hs_sync_result_free(&result);
            hs_sync_free(s);
            hs_git_free(git);
            return 1;
        }

        hs_sync_result_free(&result);
    } else {
        hs_sync_resultvec results = hs_sync_all(s, dry_run);

        int failures = 0;
        int has_conflicts = 0;

        for (size_t i = 0; i < results.len; i++) {
            hs_sync_result *r = &results.data[i];

            if (r->has_conflicts) {
                has_conflicts = 1;
                fprintf(stderr, "conflicts detected:\n");
                for (size_t j = 0; j < r->conflict_files.len; j++) {
                    fprintf(stderr, "  %s\n", r->conflict_files.data[j]);
                }
                fprintf(stderr, "\nresolve with 'hyprsync conflicts resolve' before syncing\n");
                hs_sync_resultvec_free(&results);
                hs_sync_free(s);
                hs_git_free(git);
                return 1;
            }

            if (r->success) {
                if (r->files_synced > 0 || !cli->options.quiet) {
                    printf("synced %s to %s\n", r->group_name, r->device_name);
                }
            } else {
                fprintf(stderr, "failed: %s to %s: %s\n",
                        r->group_name, r->device_name, r->error_message);
                failures++;
            }
        }

        hs_sync_resultvec_free(&results);

        if (has_conflicts || failures > 0) {
            hs_sync_free(s);
            hs_git_free(git);
            return 1;
        }
    }

    if (!dry_run) {
        printf("\nsync complete\n");
    }

    hs_sync_free(s);
    hs_git_free(git);
    return 0;
}

static int cmd_status(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized. run 'hyprsync init' first.\n");
        hs_git_free(git);
        return 1;
    }

    printf("hyprsync status\n");
    printf("  hostname: %s\n", cli->config.hostname);
    printf("  mode: %s\n", hs_sync_mode_to_string(cli->config.mode));
    printf("  devices: %zu\n", cli->config.devices.len);
    printf("  sync groups: %zu\n", cli->config.sync_groups.len);
    printf("\n");

    if (hs_git_has_conflicts(git)) {
        hs_conflictvec conflicts = hs_git_get_conflicts(git);
        printf("  conflicts: %zu\n", conflicts.len);
        for (size_t i = 0; i < conflicts.len; i++) {
            printf("    %s\n", conflicts.data[i].path);
        }
        printf("\n  resolve with 'hyprsync conflicts resolve'\n");
        hs_conflictvec_free(&conflicts);
    } else if (hs_git_has_changes(git)) {
        printf("  local changes pending:\n");
        hs_strvec files = hs_git_changed_files(git);
        for (size_t i = 0; i < files.len; i++) {
            printf("    %s\n", files.data[i]);
        }
        hs_strvec_free(&files);
    } else {
        printf("  no local changes\n");
    }

    hs_git_free(git);
    return 0;
}

static int cmd_diff(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized. run 'hyprsync init' first.\n");
        hs_git_free(git);
        return 1;
    }

    char *diff_output;

    if (cli->options.args.len > 0) {
        diff_output = hs_git_diff_remote(git, cli->options.args.data[0]);
    } else {
        diff_output = hs_git_diff(git);
    }

    if (!diff_output || diff_output[0] == '\0') {
        printf("no changes\n");
    } else {
        printf("%s", diff_output);
    }

    free(diff_output);
    hs_git_free(git);
    return 0;
}

static int cmd_log(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized. run 'hyprsync init' first.\n");
        hs_git_free(git);
        return 1;
    }

    hs_strvec entries = hs_git_log(git, 20);

    if (entries.len == 0) {
        printf("no sync history\n");
    } else {
        printf("recent sync history:\n");
        for (size_t i = 0; i < entries.len; i++) {
            printf("  %s\n", entries.data[i]);
        }
    }

    hs_strvec_free(&entries);
    hs_git_free(git);
    return 0;
}

static int cmd_ping(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    if (cli->config.devices.len == 0) {
        printf("no devices configured\n");
        return 0;
    }

    printf("testing device connectivity...\n\n");

    int failures = 0;

    for (size_t i = 0; i < cli->config.devices.len; i++) {
        const hs_device *dev = &cli->config.devices.data[i];
        printf("  %s (%s@%s:%d): ", dev->name, dev->user, dev->host, dev->port);
        fflush(stdout);

        const char *key = (dev->key && dev->key[0] != '\0')
            ? dev->key
            : cli->config.ssh.key;

        char timeout_str[32];
        snprintf(timeout_str, sizeof(timeout_str), "ConnectTimeout=%d",
                 cli->config.ssh.timeout);

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", dev->port);

        char userhost[512];
        snprintf(userhost, sizeof(userhost), "%s@%s", dev->user, dev->host);

        hs_strvec args;
        hs_vec_init(&args);
        hs_vec_push(&args, strdup("ssh"));
        hs_vec_push(&args, strdup("-i"));
        hs_vec_push(&args, strdup(key));
        hs_vec_push(&args, strdup("-p"));
        hs_vec_push(&args, strdup(port_str));
        hs_vec_push(&args, strdup("-o"));
        hs_vec_push(&args, strdup(timeout_str));
        hs_vec_push(&args, strdup("-o"));
        hs_vec_push(&args, strdup("BatchMode=yes"));
        hs_vec_push(&args, strdup("-o"));
        hs_vec_push(&args, strdup("StrictHostKeyChecking=accept-new"));
        hs_vec_push(&args, strdup("-o"));
        hs_vec_push(&args, strdup("LogLevel=ERROR"));
        hs_vec_push(&args, strdup(userhost));
        hs_vec_push(&args, strdup("echo ok"));

        hs_exec_result result = hs_exec_args(&args);
        hs_strvec_free(&args);

        char *trimmed = hs_trim(result.stdout_output);
        const char *last_line = trimmed;
        if (trimmed) {
            char *last_nl = strrchr(trimmed, '\n');
            if (last_nl) last_line = last_nl + 1;
        }
        if (hs_exec_success(&result) && last_line && strcmp(last_line, "ok") == 0) {
            printf("OK\n");
        } else {
            printf("FAILED\n");
            failures++;
        }

        free(trimmed);
        hs_exec_result_free(&result);
    }

    printf("\n");

    if (failures == 0) {
        printf("all devices reachable\n");
    } else {
        printf("%d device(s) unreachable\n", failures);
    }

    return failures > 0 ? 1 : 0;
}

static int cmd_conflicts(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized. run 'hyprsync init' first.\n");
        hs_git_free(git);
        return 1;
    }

    hs_conflictvec conflicts = hs_git_get_conflicts(git);

    if (conflicts.len == 0) {
        printf("no conflicts\n");
        hs_conflictvec_free(&conflicts);
        hs_git_free(git);
        return 0;
    }

    printf("found %zu conflict(s):\n\n", conflicts.len);

    for (size_t i = 0; i < conflicts.len; i++) {
        printf("  %s\n", conflicts.data[i].path);
    }

    printf("\n");

    int resolve_all = 0;
    if (cli->options.args.len > 0 && strcmp(cli->options.args.data[0], "resolve") == 0) {
        resolve_all = 1;
    }

    if (!resolve_all) {
        printf("run 'hyprsync conflicts resolve' to resolve interactively\n");
        printf("or 'hyprsync conflicts resolve --auto' to use configured strategy (%s)\n",
               hs_conflict_strategy_to_string(cli->config.conflict_strategy));
        hs_conflictvec_free(&conflicts);
        hs_git_free(git);
        return 0;
    }

    int auto_resolve = 0;
    for (size_t i = 0; i < cli->options.args.len; i++) {
        if (strcmp(cli->options.args.data[i], "--auto") == 0 ||
            strcmp(cli->options.args.data[i], "-a") == 0) {
            auto_resolve = 1;
            break;
        }
    }

    if (auto_resolve) {
        printf("resolving conflicts using strategy: %s\n\n",
               hs_conflict_strategy_to_string(cli->config.conflict_strategy));

        int resolved = 0;
        for (size_t i = 0; i < conflicts.len; i++) {
            int ok = hs_git_resolve_conflict(git, conflicts.data[i].path,
                                             cli->config.conflict_strategy);
            if (ok) {
                printf("  resolved: %s\n", conflicts.data[i].path);
                resolved++;
            } else {
                printf("  skipped (manual required): %s\n", conflicts.data[i].path);
            }
        }

        if (resolved > 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "hyprsync: resolved %d conflict(s)", resolved);
            hs_git_commit(git, msg);
        }

        printf("\nresolved %d of %zu conflict(s)\n", resolved, conflicts.len);
        hs_conflictvec_free(&conflicts);
        hs_git_free(git);
        return 0;
    }

    hs_tui *tui = hs_tui_create();

    hs_strvec strategies;
    hs_vec_init(&strategies);
    hs_vec_push(&strategies, strdup("newest_wins - keep the file with the most recent modification time"));
    hs_vec_push(&strategies, strdup("keep_both   - save both versions as file.local and file.remote"));
    hs_vec_push(&strategies, strdup("manual      - skip and resolve manually later"));

    for (size_t i = 0; i < conflicts.len; i++) {
        printf("\nconflict: %s\n", conflicts.data[i].path);

        int choice = hs_tui_select(tui, "resolution strategy:", &strategies, 0);

        hs_conflict_strategy strategy;
        switch (choice) {
            case 0: strategy = HS_CONFLICT_NEWEST_WINS; break;
            case 1: strategy = HS_CONFLICT_KEEP_BOTH; break;
            default: strategy = HS_CONFLICT_MANUAL; break;
        }

        if (strategy == HS_CONFLICT_MANUAL) {
            printf("  skipped\n");
            continue;
        }

        int ok = hs_git_resolve_conflict(git, conflicts.data[i].path, strategy);
        if (ok) {
            printf("  resolved\n");
        } else {
            printf("  failed to resolve\n");
        }
    }

    if (!hs_git_has_conflicts(git)) {
        hs_git_commit(git, "hyprsync: resolved conflicts");
        printf("\nall conflicts resolved\n");
    } else {
        printf("\nsome conflicts remain - resolve manually or run again\n");
    }

    hs_strvec_free(&strategies);
    hs_tui_free(tui);
    hs_conflictvec_free(&conflicts);
    hs_git_free(git);
    return 0;
}

static int cmd_restore(hs_cli *cli) {
    if (load_config(cli) != 0) {
        fprintf(stderr, "no config found. run 'hyprsync init' first.\n");
        return 1;
    }

    hs_git *git = hs_git_create(&cli->config.git, cli->config.hostname);

    if (!hs_git_is_initialized(git)) {
        fprintf(stderr, "git repo not initialized.\n");
        hs_git_free(git);
        return 1;
    }

    hs_git_restore(git, &cli->config.sync_groups);
    printf("files restored from repo\n");

    hs_git_free(git);
    return 0;
}

static int cmd_upgrade(hs_cli *cli) {
    if (cli->options.devel && cli->options.args.len == 0) {
        return hs_upgrade_to_latest_dev() ? 0 : 1;
    }

    if (cli->options.args.len == 0) {
        return hs_upgrade_to_latest() ? 0 : 1;
    }

    const char *arg = cli->options.args.data[0];

    if (strcmp(arg, "list") == 0 || strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0) {
        hs_list_available_versions();
        return 0;
    }

    if (strcmp(arg, "check") == 0 || strcmp(arg, "--check") == 0) {
        if (cli->options.devel) {
            hs_release latest;
            if (!hs_get_latest_dev_release(&latest)) {
                printf("no development builds available\n");
                return 0;
            }

            hs_version current = hs_current_version();
            char *cur_str = hs_version_to_string(current);
            printf("latest dev build: %s\n", latest.tag_name);
            printf("current version:  %s\n", cur_str);
            printf("\nrun 'hyprsync upgrade --devel' to install\n");
            free(cur_str);
            hs_release_free(&latest);
            return 0;
        }

        hs_release latest;
        if (!hs_get_latest_release(&latest)) {
            hs_version current = hs_current_version();
            char *cur_str = hs_version_to_string(current);
            printf("no stable releases available yet\n");
            printf("current version: %s\n", cur_str);
            free(cur_str);
            return 0;
        }

        hs_version current = hs_current_version();
        if (hs_version_cmp(latest.version, current) > 0) {
            char *latest_str = hs_version_to_string(latest.version);
            char *cur_str = hs_version_to_string(current);
            printf("update available: %s\n", latest_str);
            printf("current version: %s\n", cur_str);
            printf("\nrun 'hyprsync upgrade' to update\n");
            free(latest_str);
            free(cur_str);
        } else {
            char *cur_str = hs_version_to_string(current);
            printf("you are on the latest version (%s) :)\n", cur_str);
            free(cur_str);
        }

        hs_release_free(&latest);
        return 0;
    }

    return hs_upgrade_to_version(arg) ? 0 : 1;
}

static int cmd_version(hs_cli *cli) {
    (void)cli;
    printf("hyprsync %s\n", HS_VERSION);
    printf("  build: %s (%s)\n", HS_BUILD_DATE, HS_GIT_COMMIT);
    return 0;
}

static int cmd_help(hs_cli *cli) {
    (void)cli;
    print_usage();
    return 0;
}

static void print_usage(void) {
    printf("hyprsync - a lightweight sync daemon for Hyprland users\n");
    printf("\n");
    printf("usage:\n");
    printf("    hyprsync <command> [options]\n");
    printf("\n");
    printf("commands:\n");
    printf("    init              interactive setup wizard\n");
    printf("    daemon            start the sync daemon\n");
    printf("    sync              run a one-shot sync\n");
    printf("    restore           restore files from repo to original locations\n");
    printf("    status            show sync status\n");
    printf("    diff [device]     show pending changes\n");
    printf("    log               show sync history\n");
    printf("    ping              test device connectivity\n");
    printf("    conflicts         list sync conflicts\n");
    printf("    conflicts resolve resolve conflicts interactively\n");
    printf("    conflicts resolve --auto  resolve using configured strategy\n");
    printf("    upgrade [version] upgrade to latest or specific version\n");
    printf("    upgrade --devel   upgrade to latest development build\n");
    printf("    upgrade list      list available versions\n");
    printf("    upgrade check     check for updates\n");
    printf("    version           show version info\n");
    printf("\n");
    printf("options:\n");
    printf("    -c, --config <path>   config file path\n");
    printf("    -n, --dry-run         show what would be synced\n");
    printf("    -v, --verbose         enable verbose output\n");
    printf("    -q, --quiet           suppress non-error output\n");
    printf("    -g, --group <name>    only sync a specific group\n");
    printf("    -d, --device <name>   only sync with a specific device\n");
    printf("    -h, --help            show this help\n");
    printf("\n");
    printf("made with \xF0\x9F\xA7\x80 by r2unit\n");
}
