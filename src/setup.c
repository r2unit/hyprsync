#define _POSIX_C_SOURCE 200809L

#include "setup.h"
#include "config.h"
#include "git.h"
#include "log.h"
#include "tui.h"
#include "util.h"
#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *CONFIG_SCAN[] = {
    "hypr", "waybar", "wofi", "dunst", "rofi", "kitty",
    "alacritty", "nvim", "neovim", "foot", "sway", "i3",
    "polybar", "picom", "zsh", "fish", "tmux", "wezterm",
    "mako", "swaylock", "hyprlock", "hypridle", "hyprpaper",
    "starship.toml", "fastfetch",
};
static const size_t CONFIG_SCAN_COUNT = sizeof(CONFIG_SCAN) / sizeof(CONFIG_SCAN[0]);

static const char *HOME_SCAN[] = {
    ".zshrc", ".zshenv", ".bashrc", ".bash_profile",
    ".gitconfig", ".tmux.conf", ".vimrc", ".nanorc",
};
static const size_t HOME_SCAN_COUNT = sizeof(HOME_SCAN) / sizeof(HOME_SCAN[0]);

static const char *LOCAL_SCAN[] = {
    "bin",
};
static const size_t LOCAL_SCAN_COUNT = sizeof(LOCAL_SCAN) / sizeof(LOCAL_SCAN[0]);

static const char *HYPRLAND_ITEMS[] = {
    "hypr", "waybar", "dunst", "hyprlock", "hypridle", "hyprpaper",
};
static const size_t HYPRLAND_ITEMS_COUNT = sizeof(HYPRLAND_ITEMS) / sizeof(HYPRLAND_ITEMS[0]);

static int is_hyprland_item(const char *name) {
    for (size_t i = 0; i < HYPRLAND_ITEMS_COUNT; i++) {
        if (strcmp(HYPRLAND_ITEMS[i], name) == 0)
            return 1;
    }
    return 0;
}

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static char *prompt_hostname(hs_tui *tui) {
    char *detected = hs_get_hostname();
    char *result = hs_tui_prompt(tui, "Hostname for this machine", detected);
    free(detected);
    return result;
}

static void prompt_ssh(hs_tui *tui, char **out_key, int *out_port) {
    char *ssh_dir = hs_expand_path("~/.ssh");
    hs_strvec key_paths;
    hs_vec_init(&key_paths);

    static const char *key_names[] = {
        "id_ed25519", "id_rsa", "id_ecdsa", "id_dsa"
    };
    static const size_t key_names_count = 4;

    if (hs_dir_exists(ssh_dir)) {
        for (size_t i = 0; i < key_names_count; i++) {
            char *path = hs_join_path(ssh_dir, key_names[i]);
            if (hs_file_exists(path)) {
                hs_vec_push(&key_paths, path);
            } else {
                free(path);
            }
        }
    }

    char *selected_key = NULL;

    if (key_paths.len > 0) {
        hs_strvec display_names;
        hs_vec_init(&display_names);
        int default_idx = 0;

        for (size_t i = 0; i < key_paths.len; i++) {
            const char *name = basename_of(key_paths.data[i]);
            hs_vec_push(&display_names, strdup(name));
            if (strcmp(name, "id_ed25519") == 0)
                default_idx = (int)i;
        }

        hs_tui_print_info("Found SSH keys:");
        int idx = hs_tui_select(tui, "Select SSH key", &display_names, default_idx);
        selected_key = strdup(key_paths.data[idx]);
        hs_strvec_free(&display_names);
    } else {
        char *key_path = hs_tui_prompt(tui, "SSH key path", "~/.ssh/id_ed25519");
        selected_key = hs_expand_path(key_path);
        free(key_path);
    }

    hs_strvec_free(&key_paths);
    free(ssh_dir);

    char *port_str = hs_tui_prompt(tui, "Default SSH port", "22");
    int port = 22;
    if (port_str) {
        char *endptr = NULL;
        long val = strtol(port_str, &endptr, 10);
        if (endptr != port_str && *endptr == '\0')
            port = (int)val;
        free(port_str);
    }

    *out_key = selected_key;
    *out_port = port;
}

static int test_connection(const hs_device *device, const char *key) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", device->port);

    size_t userhost_len = strlen(device->user) + 1 + strlen(device->host) + 1;
    char *userhost = malloc(userhost_len);
    snprintf(userhost, userhost_len, "%s@%s", device->user, device->host);

    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("ssh"));
    hs_vec_push(&args, strdup("-i"));
    hs_vec_push(&args, strdup(key));
    hs_vec_push(&args, strdup("-p"));
    hs_vec_push(&args, strdup(port_str));
    hs_vec_push(&args, strdup("-o"));
    hs_vec_push(&args, strdup("ConnectTimeout=5"));
    hs_vec_push(&args, strdup("-o"));
    hs_vec_push(&args, strdup("BatchMode=yes"));
    hs_vec_push(&args, strdup("-o"));
    hs_vec_push(&args, strdup("StrictHostKeyChecking=accept-new"));
    hs_vec_push(&args, strdup("-o"));
    hs_vec_push(&args, strdup("LogLevel=ERROR"));
    hs_vec_push(&args, userhost);
    hs_vec_push(&args, strdup("echo ok"));

    hs_exec_result result = hs_exec_args(&args);
    hs_strvec_free(&args);

    int ok = 0;
    if (hs_exec_success(&result)) {
        char *trimmed = hs_trim(result.stdout_output);
        if (trimmed) {
            char *last_nl = strrchr(trimmed, '\n');
            const char *last_line = last_nl ? last_nl + 1 : trimmed;
            if (strcmp(last_line, "ok") == 0)
                ok = 1;
        }
        free(trimmed);
    }
    hs_exec_result_free(&result);
    return ok;
}

static hs_devicevec prompt_devices(hs_tui *tui, const char *key, int default_port) {
    hs_devicevec devices;
    hs_vec_init(&devices);

    int add_more = 1;
    while (add_more) {
        hs_tui_print_info("Add a device to sync with:");

        hs_device device;
        memset(&device, 0, sizeof(device));

        device.name = hs_tui_prompt(tui, "  Name", NULL);
        if (!device.name || device.name[0] == '\0') {
            free(device.name);
            break;
        }

        device.host = hs_tui_prompt(tui, "  Host (IP or hostname)", NULL);
        device.user = hs_tui_prompt(tui, "  User", NULL);

        char port_default[16];
        snprintf(port_default, sizeof(port_default), "%d", default_port);
        char *port_str = hs_tui_prompt(tui, "  Port", port_default);
        device.port = default_port;
        if (port_str) {
            char *endptr = NULL;
            long val = strtol(port_str, &endptr, 10);
            if (endptr != port_str && *endptr == '\0')
                device.port = (int)val;
            free(port_str);
        }

        device.key = NULL;

        hs_tui_print_info("  Testing connection...");
        if (test_connection(&device, key)) {
            hs_tui_print_success("Connection OK");
            hs_vec_push(&devices, device);
        } else {
            hs_tui_print_error("Connection failed");
            if (hs_tui_confirm(tui, "  Add device anyway?", 0)) {
                hs_vec_push(&devices, device);
            } else {
                hs_device_free(&device);
            }
        }

        add_more = hs_tui_confirm(tui, "Add another device?", 0);
    }

    return devices;
}

static hs_strvec scan_config_dirs(void) {
    hs_strvec found;
    hs_vec_init(&found);
    char *config_dir = hs_expand_path("~/.config");

    if (!hs_dir_exists(config_dir)) {
        free(config_dir);
        return found;
    }

    for (size_t i = 0; i < CONFIG_SCAN_COUNT; i++) {
        char *path = hs_join_path(config_dir, CONFIG_SCAN[i]);
        if (hs_dir_exists(path) || hs_file_exists(path)) {
            hs_vec_push(&found, path);
        } else {
            free(path);
        }
    }

    free(config_dir);
    return found;
}

static hs_strvec scan_dotfiles(void) {
    hs_strvec found;
    hs_vec_init(&found);
    char *home = hs_get_home_dir();

    if (!home) return found;

    for (size_t i = 0; i < HOME_SCAN_COUNT; i++) {
        char *path = hs_join_path(home, HOME_SCAN[i]);
        if (hs_file_exists(path)) {
            hs_vec_push(&found, path);
        } else {
            free(path);
        }
    }

    free(home);
    return found;
}

static hs_strvec scan_local_dirs(void) {
    hs_strvec found;
    hs_vec_init(&found);
    char *local_dir = hs_expand_path("~/.local");

    if (!hs_dir_exists(local_dir)) {
        free(local_dir);
        return found;
    }

    for (size_t i = 0; i < LOCAL_SCAN_COUNT; i++) {
        char *path = hs_join_path(local_dir, LOCAL_SCAN[i]);
        if (hs_dir_exists(path)) {
            hs_vec_push(&found, path);
        } else {
            free(path);
        }
    }

    free(local_dir);
    return found;
}


static hs_groupvec prompt_sync_paths(hs_tui *tui) {
    hs_tui_print_info("Scanning for dotfiles...");
    hs_tui_print_blank();

    hs_strvec config_dirs = scan_config_dirs();
    hs_strvec dotfiles = scan_dotfiles();
    hs_strvec local_dirs = scan_local_dirs();

    hs_groupvec groups;
    hs_vec_init(&groups);

    if (config_dirs.len > 0) {
        hs_tui_print_info("Config directories:");

        hs_strvec items;
        hs_vec_init(&items);
        int *defaults = calloc(config_dirs.len, sizeof(int));

        for (size_t i = 0; i < config_dirs.len; i++) {
            const char *name = basename_of(config_dirs.data[i]);
            size_t label_len = strlen("~/.config/") + strlen(name) + 2;
            char *label = malloc(label_len);
            snprintf(label, label_len, "~/.config/%s/", name);
            hs_vec_push(&items, label);
            defaults[i] = is_hyprland_item(name);
        }

        hs_sizevec selected = hs_tui_checkbox(tui, "Select directories", &items, defaults);

        if (selected.len > 0) {
            hs_sync_group group;
            memset(&group, 0, sizeof(group));
            group.name = strdup("config");
            hs_vec_init(&group.paths);
            hs_vec_init(&group.exclude);
            hs_vec_init(&group.devices);
            group.remote_path = NULL;

            for (size_t i = 0; i < selected.len; i++) {
                hs_vec_push(&group.paths, strdup(config_dirs.data[selected.data[i]]));
            }

            hs_vec_push(&group.exclude, strdup("*.log"));
            hs_vec_push(&group.exclude, strdup("*.bak"));
            hs_vec_push(&group.exclude, strdup("__pycache__/"));
            hs_vec_push(&groups, group);
        }

        hs_vec_free(&selected);
        free(defaults);
        hs_strvec_free(&items);
    }

    if (dotfiles.len > 0) {
        hs_tui_print_blank();
        hs_tui_print_info("Dotfiles:");

        hs_strvec items;
        hs_vec_init(&items);
        int *defaults = calloc(dotfiles.len, sizeof(int));

        for (size_t i = 0; i < dotfiles.len; i++) {
            const char *name = basename_of(dotfiles.data[i]);
            size_t label_len = strlen("~/") + strlen(name) + 1;
            char *label = malloc(label_len);
            snprintf(label, label_len, "~/%s", name);
            hs_vec_push(&items, label);
            defaults[i] = 1;
        }

        hs_sizevec selected = hs_tui_checkbox(tui, "Select dotfiles", &items, defaults);

        if (selected.len > 0) {
            hs_sync_group group;
            memset(&group, 0, sizeof(group));
            group.name = strdup("dotfiles");
            hs_vec_init(&group.paths);
            hs_vec_init(&group.exclude);
            hs_vec_init(&group.devices);
            group.remote_path = NULL;

            for (size_t i = 0; i < selected.len; i++) {
                hs_vec_push(&group.paths, strdup(dotfiles.data[selected.data[i]]));
            }

            hs_vec_push(&groups, group);
        }

        hs_vec_free(&selected);
        free(defaults);
        hs_strvec_free(&items);
    }

    if (local_dirs.len > 0) {
        hs_tui_print_blank();
        hs_tui_print_info("Local directories:");

        hs_strvec items;
        hs_vec_init(&items);
        int *defaults = calloc(local_dirs.len, sizeof(int));

        for (size_t i = 0; i < local_dirs.len; i++) {
            const char *name = basename_of(local_dirs.data[i]);
            size_t label_len = strlen("~/.local/") + strlen(name) + 2;
            char *label = malloc(label_len);
            snprintf(label, label_len, "~/.local/%s/", name);
            hs_vec_push(&items, label);
            defaults[i] = 1;
        }

        hs_sizevec selected = hs_tui_checkbox(tui, "Select directories", &items, defaults);

        if (selected.len > 0) {
            hs_sync_group group;
            memset(&group, 0, sizeof(group));
            group.name = strdup("local");
            hs_vec_init(&group.paths);
            hs_vec_init(&group.exclude);
            hs_vec_init(&group.devices);
            group.remote_path = NULL;

            for (size_t i = 0; i < selected.len; i++) {
                hs_vec_push(&group.paths, strdup(local_dirs.data[selected.data[i]]));
            }

            hs_vec_push(&group.exclude, strdup("__pycache__/"));
            hs_vec_push(&group.exclude, strdup("*.pyc"));
            hs_vec_push(&groups, group);
        }

        hs_vec_free(&selected);
        free(defaults);
        hs_strvec_free(&items);
    }

    hs_tui_print_blank();
    if (hs_tui_confirm(tui, "Add a custom path?", 0)) {
        hs_sync_group custom_group;
        memset(&custom_group, 0, sizeof(custom_group));
        custom_group.name = strdup("custom");
        hs_vec_init(&custom_group.paths);
        hs_vec_init(&custom_group.exclude);
        hs_vec_init(&custom_group.devices);
        custom_group.remote_path = NULL;

        int add_more = 1;
        while (add_more) {
            char *path = hs_tui_prompt(tui, "  Path", NULL);
            if (path && path[0] != '\0') {
                char *expanded = hs_expand_path(path);
                hs_vec_push(&custom_group.paths, expanded);
                free(path);
                add_more = hs_tui_confirm(tui, "Add another?", 0);
            } else {
                free(path);
                add_more = 0;
            }
        }

        if (custom_group.paths.len > 0) {
            hs_vec_push(&groups, custom_group);
        } else {
            hs_sync_group_free(&custom_group);
        }
    }

    hs_strvec_free(&config_dirs);
    hs_strvec_free(&dotfiles);
    hs_strvec_free(&local_dirs);

    return groups;
}

static hs_sync_mode prompt_mode(hs_tui *tui) {
    hs_strvec options;
    hs_vec_init(&options);
    hs_vec_push(&options, strdup("bidirectional - sync changes both ways"));
    hs_vec_push(&options, strdup("push - only push local changes to remotes"));
    hs_vec_push(&options, strdup("pull - only pull changes from remotes"));

    int idx = hs_tui_select(tui, "Sync mode", &options, 0);
    hs_strvec_free(&options);

    switch (idx) {
    case 1: return HS_SYNC_PUSH;
    case 2: return HS_SYNC_PULL;
    default: return HS_SYNC_BIDIRECTIONAL;
    }
}

static hs_conflict_strategy prompt_conflict_strategy(hs_tui *tui) {
    hs_strvec options;
    hs_vec_init(&options);
    hs_vec_push(&options, strdup("newest_wins - file with latest modification time wins"));
    hs_vec_push(&options, strdup("manual - pause and ask for resolution"));
    hs_vec_push(&options, strdup("keep_both - save both versions with hostname suffix"));

    int idx = hs_tui_select(tui, "Conflict resolution strategy", &options, 0);
    hs_strvec_free(&options);

    switch (idx) {
    case 1: return HS_CONFLICT_MANUAL;
    case 2: return HS_CONFLICT_KEEP_BOTH;
    default: return HS_CONFLICT_NEWEST_WINS;
    }
}

static void write_config(const hs_config *config, const char *path) {
    hs_save_config(config, path);
}

static void init_git_repo(const hs_config *config) {
    hs_git *git = hs_git_create(&config->git, config->hostname);

    if (!hs_git_init_repo(git)) {
        hs_error("failed to initialize git repository");
        hs_git_free(git);
        return;
    }

    hs_git_snapshot(git, &config->sync_groups);

    size_t msg_len = strlen("hyprsync: initial snapshot from ") + strlen(config->hostname) + 1;
    char *message = malloc(msg_len);
    snprintf(message, msg_len, "hyprsync: initial snapshot from %s", config->hostname);

    hs_git_commit(git, message);
    free(message);
    hs_git_free(git);
}

static void print_summary(hs_tui *tui, const hs_config *config) {
    (void)tui;
    hs_tui_print_blank();
    hs_tui_print_info("Summary:");
    hs_tui_print_blank();

    char machine_line[512];
    snprintf(machine_line, sizeof(machine_line), "  Machine:    %s", config->hostname);
    hs_tui_print_info(machine_line);

    char devices_buf[1024];
    devices_buf[0] = '\0';
    size_t pos = 0;

    for (size_t i = 0; i < config->devices.len; i++) {
        const hs_device *d = &config->devices.data[i];
        if (i > 0) {
            pos += (size_t)snprintf(devices_buf + pos, sizeof(devices_buf) - pos, ", ");
        }
        pos += (size_t)snprintf(devices_buf + pos, sizeof(devices_buf) - pos,
                                "%s (%s@%s)", d->name, d->user, d->host);
        if (pos >= sizeof(devices_buf) - 1) break;
    }

    if (config->devices.len == 0)
        snprintf(devices_buf, sizeof(devices_buf), "(none)");

    char devices_line[1200];
    snprintf(devices_line, sizeof(devices_line), "  Devices:    %s", devices_buf);
    hs_tui_print_info(devices_line);

    size_t total_paths = 0;
    for (size_t i = 0; i < config->sync_groups.len; i++) {
        total_paths += config->sync_groups.data[i].paths.len;
    }

    char sync_line[256];
    snprintf(sync_line, sizeof(sync_line),
             "  Syncing:    %zu paths in %zu groups",
             total_paths, config->sync_groups.len);
    hs_tui_print_info(sync_line);

    char mode_line[256];
    snprintf(mode_line, sizeof(mode_line),
             "  Mode:       %s", hs_sync_mode_to_string(config->mode));
    hs_tui_print_info(mode_line);

    char conflict_line[256];
    snprintf(conflict_line, sizeof(conflict_line),
             "  Conflicts:  %s", hs_conflict_strategy_to_string(config->conflict_strategy));
    hs_tui_print_info(conflict_line);

    hs_tui_print_info("  Git repo:   ~/.local/share/hyprsync/");
    hs_tui_print_blank();
}

static void print_next_steps(void) {
    hs_tui_print_info("Next steps:");
    hs_tui_print_info("  hyprsync ping          # Verify device connectivity");
    hs_tui_print_info("  hyprsync sync          # First sync");
    hs_tui_print_info("  hyprsync daemon        # Start background daemon");
    hs_tui_print_blank();
}

hs_config hs_setup_run(void) {
    hs_config config;
    memset(&config, 0, sizeof(config));

    hs_tui *tui = hs_tui_create();

    hs_tui_print_header("HyprSync Setup");

    hs_tui_print_step(1, "Machine Identity");
    config.hostname = prompt_hostname(tui);
    hs_tui_print_blank();

    hs_tui_print_step(2, "SSH Configuration");
    char *ssh_key = NULL;
    int ssh_port = 22;
    prompt_ssh(tui, &ssh_key, &ssh_port);
    config.ssh.key = ssh_key;
    config.ssh.port = ssh_port;
    config.ssh.timeout = 10;
    hs_tui_print_blank();

    hs_tui_print_step(3, "Devices");
    config.devices = prompt_devices(tui, config.ssh.key, config.ssh.port);
    hs_tui_print_blank();

    hs_tui_print_step(4, "Select paths to sync");
    config.sync_groups = prompt_sync_paths(tui);
    hs_tui_print_blank();

    hs_tui_print_step(5, "Sync mode");
    config.mode = prompt_mode(tui);
    hs_tui_print_blank();

    hs_tui_print_step(6, "Conflict resolution");
    config.conflict_strategy = prompt_conflict_strategy(tui);
    hs_tui_print_blank();

    config.git.repo = hs_default_repo_path();
    config.git.auto_commit = 1;
    config.git.commit_template = strdup("hyprsync: update from $hostname");

    config.poll_interval = 0;
    config.dry_run = 0;
    config.log_level = NULL;
    config.hooks.pre_sync = NULL;
    config.hooks.post_sync = NULL;
    hs_vec_init(&config.hooks.group_hooks);

    hs_tui_print_line();
    print_summary(tui, &config);
    hs_tui_print_line();

    char *config_path = hs_default_config_path();

    if (hs_tui_confirm(tui, "Write config to ~/.config/hypr/hyprsync.toml?", 1)) {
        write_config(&config, config_path);
        hs_tui_print_success("Config written");

        init_git_repo(&config);
        hs_tui_print_success("Git repository initialized");

        hs_tui_print_blank();
        print_next_steps();
    } else {
        hs_tui_print_info("Setup cancelled");
    }

    free(config_path);
    hs_tui_free(tui);

    return config;
}
