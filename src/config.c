#include "config.h"
#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tomlc17.h>

const char *hs_sync_mode_to_string(hs_sync_mode mode) {
    switch (mode) {
    case HS_SYNC_PUSH: return "push";
    case HS_SYNC_PULL: return "pull";
    case HS_SYNC_BIDIRECTIONAL: return "bidirectional";
    }
    return "bidirectional";
}

hs_sync_mode hs_sync_mode_from_string(const char *str) {
    if (str && strcmp(str, "push") == 0) return HS_SYNC_PUSH;
    if (str && strcmp(str, "pull") == 0) return HS_SYNC_PULL;
    return HS_SYNC_BIDIRECTIONAL;
}

const char *hs_conflict_strategy_to_string(hs_conflict_strategy strategy) {
    switch (strategy) {
    case HS_CONFLICT_NEWEST_WINS: return "newest_wins";
    case HS_CONFLICT_MANUAL: return "manual";
    case HS_CONFLICT_KEEP_BOTH: return "keep_both";
    }
    return "newest_wins";
}

hs_conflict_strategy hs_conflict_strategy_from_string(const char *str) {
    if (str && strcmp(str, "manual") == 0) return HS_CONFLICT_MANUAL;
    if (str && strcmp(str, "keep_both") == 0) return HS_CONFLICT_KEEP_BOTH;
    return HS_CONFLICT_NEWEST_WINS;
}

static char *get_string_or(toml_datum_t tbl, const char *key, const char *def) {
    toml_datum_t d = toml_get(tbl, key);
    if (d.type == TOML_STRING) return strdup(d.u.s);
    return strdup(def);
}

static int64_t get_int_or(toml_datum_t tbl, const char *key, int64_t def) {
    toml_datum_t d = toml_get(tbl, key);
    if (d.type == TOML_INT64) return d.u.int64;
    return def;
}

static int get_bool_or(toml_datum_t tbl, const char *key, int def) {
    toml_datum_t d = toml_get(tbl, key);
    if (d.type == TOML_BOOLEAN) return d.u.boolean ? 1 : 0;
    return def;
}

hs_config hs_load_config(const char *path) {
    hs_config config;
    memset(&config, 0, sizeof(config));

    char *expanded = hs_expand_path(path);
    if (!hs_file_exists(expanded)) {
        hs_error("config file not found: %s", expanded);
        free(expanded);
        return config;
    }

    toml_result_t result = toml_parse_file_ex(expanded);
    free(expanded);

    if (!result.ok) {
        hs_error("failed to parse config: %s", result.errmsg);
        return config;
    }

    toml_datum_t root = result.toptab;

    toml_datum_t general = toml_get(root, "general");
    if (general.type == TOML_TABLE) {
        toml_datum_t hostname_d = toml_get(general, "hostname");
        if (hostname_d.type == TOML_STRING)
            config.hostname = strdup(hostname_d.u.s);
        else
            config.hostname = hs_get_hostname();

        char *mode_str = get_string_or(general, "mode", "bidirectional");
        config.mode = hs_sync_mode_from_string(mode_str);
        free(mode_str);

        char *cs_str = get_string_or(general, "conflict_strategy", "newest_wins");
        config.conflict_strategy = hs_conflict_strategy_from_string(cs_str);
        free(cs_str);

        config.poll_interval = (int)get_int_or(general, "poll_interval", 0);
        config.dry_run = get_bool_or(general, "dry_run", 0);
        config.log_level = get_string_or(general, "log_level", "info");
    } else {
        config.hostname = hs_get_hostname();
        config.log_level = strdup("info");
    }

    toml_datum_t git = toml_get(root, "git");
    if (git.type == TOML_TABLE) {
        char *repo_str = get_string_or(git, "repo", "~/.local/share/hyprsync");
        config.git.repo = hs_expand_path(repo_str);
        free(repo_str);
        config.git.auto_commit = get_bool_or(git, "auto_commit", 1);
        config.git.commit_template = get_string_or(git, "commit_template",
                                                     "hyprsync: update from $hostname");
    } else {
        config.git.repo = hs_expand_path("~/.local/share/hyprsync");
        config.git.auto_commit = 1;
        config.git.commit_template = strdup("hyprsync: update from $hostname");
    }

    toml_datum_t ssh = toml_get(root, "ssh");
    if (ssh.type == TOML_TABLE) {
        char *key_str = get_string_or(ssh, "key", "~/.ssh/id_ed25519");
        config.ssh.key = hs_expand_path(key_str);
        free(key_str);
        config.ssh.port = (int)get_int_or(ssh, "port", 22);
        config.ssh.timeout = (int)get_int_or(ssh, "timeout", 10);
    } else {
        config.ssh.key = hs_expand_path("~/.ssh/id_ed25519");
        config.ssh.port = 22;
        config.ssh.timeout = 10;
    }

    hs_vec_init(&config.devices);
    toml_datum_t devices = toml_get(root, "device");
    if (devices.type == TOML_ARRAY) {
        for (int i = 0; i < devices.u.arr.size; i++) {
            toml_datum_t dev_tbl = devices.u.arr.elem[i];
            if (dev_tbl.type != TOML_TABLE) continue;

            hs_device device;
            memset(&device, 0, sizeof(device));

            device.name = get_string_or(dev_tbl, "name", "");
            device.host = get_string_or(dev_tbl, "host", "");
            device.user = get_string_or(dev_tbl, "user", "");
            device.port = (int)get_int_or(dev_tbl, "port", config.ssh.port);

            toml_datum_t key_d = toml_get(dev_tbl, "key");
            if (key_d.type == TOML_STRING && key_d.u.s[0] != '\0') {
                device.key = hs_expand_path(key_d.u.s);
            } else {
                device.key = strdup("");
            }

            if (device.name[0] != '\0' && device.host[0] != '\0') {
                hs_vec_push(&config.devices, device);
            } else {
                hs_device_free(&device);
            }
        }
    }

    hs_vec_init(&config.sync_groups);
    toml_datum_t syncs = toml_get(root, "sync");
    if (syncs.type == TOML_ARRAY) {
        for (int i = 0; i < syncs.u.arr.size; i++) {
            toml_datum_t sync_tbl = syncs.u.arr.elem[i];
            if (sync_tbl.type != TOML_TABLE) continue;

            hs_sync_group group;
            memset(&group, 0, sizeof(group));
            group.name = get_string_or(sync_tbl, "name", "");
            hs_vec_init(&group.paths);
            hs_vec_init(&group.exclude);
            hs_vec_init(&group.devices);

            toml_datum_t paths = toml_get(sync_tbl, "paths");
            if (paths.type == TOML_ARRAY) {
                for (int j = 0; j < paths.u.arr.size; j++) {
                    toml_datum_t s = paths.u.arr.elem[j];
                    if (s.type == TOML_STRING) {
                        char *exp = hs_expand_path(s.u.s);
                        hs_vec_push(&group.paths, exp);
                    }
                }
            }

            toml_datum_t excludes = toml_get(sync_tbl, "exclude");
            if (excludes.type == TOML_ARRAY) {
                for (int j = 0; j < excludes.u.arr.size; j++) {
                    toml_datum_t s = excludes.u.arr.elem[j];
                    if (s.type == TOML_STRING)
                        hs_vec_push(&group.exclude, strdup(s.u.s));
                }
            }

            toml_datum_t devs = toml_get(sync_tbl, "devices");
            if (devs.type == TOML_ARRAY) {
                for (int j = 0; j < devs.u.arr.size; j++) {
                    toml_datum_t s = devs.u.arr.elem[j];
                    if (s.type == TOML_STRING)
                        hs_vec_push(&group.devices, strdup(s.u.s));
                }
            }

            group.remote_path = get_string_or(sync_tbl, "remote_path", "");

            if (group.name[0] != '\0' && group.paths.len > 0) {
                hs_vec_push(&config.sync_groups, group);
            } else {
                hs_sync_group_free(&group);
            }
        }
    }

    config.hooks.pre_sync = strdup("");
    config.hooks.post_sync = strdup("");
    hs_vec_init(&config.hooks.group_hooks);

    toml_datum_t hooks = toml_get(root, "hooks");
    if (hooks.type == TOML_TABLE) {
        toml_datum_t pre = toml_get(hooks, "pre_sync");
        if (pre.type == TOML_STRING) {
            free(config.hooks.pre_sync);
            config.hooks.pre_sync = strdup(pre.u.s);
        }
        toml_datum_t post = toml_get(hooks, "post_sync");
        if (post.type == TOML_STRING) {
            free(config.hooks.post_sync);
            config.hooks.post_sync = strdup(post.u.s);
        }

        toml_datum_t group_hooks = toml_get(hooks, "group");
        if (group_hooks.type == TOML_TABLE) {
            for (int i = 0; i < group_hooks.u.tab.size; i++) {
                toml_datum_t val = group_hooks.u.tab.value[i];
                if (val.type == TOML_STRING) {
                    hs_hook_entry entry;
                    entry.key = strdup(group_hooks.u.tab.key[i]);
                    entry.value = strdup(val.u.s);
                    hs_vec_push(&config.hooks.group_hooks, entry);
                }
            }
        }
    }

    toml_free(result);
    return config;
}

static void mkdirs(const char *path) {
    char *tmp = strdup(path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    free(tmp);
}

static char *dir_of(const char *path) {
    char *dup = strdup(path);
    char *last = strrchr(dup, '/');
    if (last) {
        *last = '\0';
        return dup;
    }
    free(dup);
    return NULL;
}

void hs_save_config(const hs_config *config, const char *path) {
    char *expanded = hs_expand_path(path);

    char *parent = dir_of(expanded);
    if (parent && parent[0] != '\0' && !hs_dir_exists(parent))
        mkdirs(parent);
    free(parent);

    FILE *fp = fopen(expanded, "w");
    if (!fp) {
        hs_error("failed to open config file for writing: %s", expanded);
        free(expanded);
        return;
    }
    free(expanded);

    fprintf(fp, "# HyprSync Configuration\n");
    fprintf(fp, "# Generated by: hyprsync init\n\n");

    fprintf(fp, "[general]\n");
    fprintf(fp, "hostname = \"%s\"\n", config->hostname ? config->hostname : "");
    fprintf(fp, "mode = \"%s\"\n", hs_sync_mode_to_string(config->mode));
    fprintf(fp, "conflict_strategy = \"%s\"\n", hs_conflict_strategy_to_string(config->conflict_strategy));
    fprintf(fp, "poll_interval = %d\n", config->poll_interval);
    fprintf(fp, "dry_run = %s\n", config->dry_run ? "true" : "false");
    fprintf(fp, "log_level = \"%s\"\n", config->log_level ? config->log_level : "info");
    fprintf(fp, "\n");

    fprintf(fp, "[git]\n");
    fprintf(fp, "repo = \"%s\"\n", config->git.repo ? config->git.repo : "");
    fprintf(fp, "auto_commit = %s\n", config->git.auto_commit ? "true" : "false");
    fprintf(fp, "commit_template = \"%s\"\n", config->git.commit_template ? config->git.commit_template : "");
    fprintf(fp, "\n");

    fprintf(fp, "[ssh]\n");
    fprintf(fp, "key = \"%s\"\n", config->ssh.key ? config->ssh.key : "");
    fprintf(fp, "port = %d\n", config->ssh.port);
    fprintf(fp, "timeout = %d\n", config->ssh.timeout);
    fprintf(fp, "\n");

    for (size_t i = 0; i < config->devices.len; i++) {
        hs_device *d = &config->devices.data[i];
        fprintf(fp, "[[device]]\n");
        fprintf(fp, "name = \"%s\"\n", d->name);
        fprintf(fp, "host = \"%s\"\n", d->host);
        fprintf(fp, "user = \"%s\"\n", d->user);
        fprintf(fp, "port = %d\n", d->port);
        if (d->key && d->key[0] != '\0')
            fprintf(fp, "key = \"%s\"\n", d->key);
        fprintf(fp, "\n");
    }

    for (size_t i = 0; i < config->sync_groups.len; i++) {
        hs_sync_group *g = &config->sync_groups.data[i];
        fprintf(fp, "[[sync]]\n");
        fprintf(fp, "name = \"%s\"\n", g->name);
        fprintf(fp, "paths = [\n");
        for (size_t j = 0; j < g->paths.len; j++)
            fprintf(fp, "    \"%s\",\n", g->paths.data[j]);
        fprintf(fp, "]\n");

        if (g->exclude.len > 0) {
            fprintf(fp, "exclude = [\n");
            for (size_t j = 0; j < g->exclude.len; j++)
                fprintf(fp, "    \"%s\",\n", g->exclude.data[j]);
            fprintf(fp, "]\n");
        }

        if (g->devices.len > 0) {
            fprintf(fp, "devices = [");
            for (size_t j = 0; j < g->devices.len; j++) {
                if (j > 0) fprintf(fp, ", ");
                fprintf(fp, "\"%s\"", g->devices.data[j]);
            }
            fprintf(fp, "]\n");
        }

        if (g->remote_path && g->remote_path[0] != '\0')
            fprintf(fp, "remote_path = \"%s\"\n", g->remote_path);
        fprintf(fp, "\n");
    }

    fprintf(fp, "[hooks]\n");
    fprintf(fp, "pre_sync = \"%s\"\n", config->hooks.pre_sync ? config->hooks.pre_sync : "");
    fprintf(fp, "post_sync = \"%s\"\n", config->hooks.post_sync ? config->hooks.post_sync : "");
    fprintf(fp, "\n");

    if (config->hooks.group_hooks.len > 0) {
        fprintf(fp, "[hooks.group]\n");
        for (size_t i = 0; i < config->hooks.group_hooks.len; i++) {
            hs_hook_entry *e = &config->hooks.group_hooks.data[i];
            fprintf(fp, "%s = \"%s\"\n", e->key, e->value);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
}

void hs_device_free(hs_device *d) {
    free(d->name);
    free(d->host);
    free(d->user);
    free(d->key);
    d->name = NULL;
    d->host = NULL;
    d->user = NULL;
    d->key = NULL;
}

void hs_sync_group_free(hs_sync_group *g) {
    free(g->name);
    hs_strvec_free(&g->paths);
    hs_strvec_free(&g->exclude);
    hs_strvec_free(&g->devices);
    free(g->remote_path);
    g->name = NULL;
    g->remote_path = NULL;
}

void hs_config_free(hs_config *config) {
    free(config->hostname);
    free(config->log_level);

    free(config->git.repo);
    free(config->git.commit_template);

    free(config->ssh.key);

    for (size_t i = 0; i < config->devices.len; i++)
        hs_device_free(&config->devices.data[i]);
    hs_vec_free(&config->devices);

    for (size_t i = 0; i < config->sync_groups.len; i++)
        hs_sync_group_free(&config->sync_groups.data[i]);
    hs_vec_free(&config->sync_groups);

    free(config->hooks.pre_sync);
    free(config->hooks.post_sync);
    for (size_t i = 0; i < config->hooks.group_hooks.len; i++) {
        free(config->hooks.group_hooks.data[i].key);
        free(config->hooks.group_hooks.data[i].value);
    }
    hs_vec_free(&config->hooks.group_hooks);

    memset(config, 0, sizeof(*config));
}

char *hs_default_config_path(void) {
    return hs_expand_path("~/.config/hypr/hyprsync.toml");
}

char *hs_default_repo_path(void) {
    return hs_expand_path("~/.local/share/hyprsync/repo");
}
