#include "config.h"
#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <toml.h>

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

static char *toml_string_or(toml_table_t *tbl, const char *key, const char *def) {
    toml_datum_t d = toml_string_in(tbl, key);
    if (d.ok) return d.u.s;
    return strdup(def);
}

static int64_t toml_int_or(toml_table_t *tbl, const char *key, int64_t def) {
    toml_datum_t d = toml_int_in(tbl, key);
    if (d.ok) return d.u.i;
    return def;
}

static int toml_bool_or(toml_table_t *tbl, const char *key, int def) {
    toml_datum_t d = toml_bool_in(tbl, key);
    if (d.ok) return d.u.b;
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

    FILE *fp = fopen(expanded, "r");
    if (!fp) {
        hs_error("failed to open config file: %s", expanded);
        free(expanded);
        return config;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    free(expanded);

    if (!root) {
        hs_error("failed to parse config: %s", errbuf);
        return config;
    }

    toml_table_t *general = toml_table_in(root, "general");
    if (general) {
        toml_datum_t hostname_d = toml_string_in(general, "hostname");
        if (hostname_d.ok)
            config.hostname = hostname_d.u.s;
        else
            config.hostname = hs_get_hostname();

        char *mode_str = toml_string_or(general, "mode", "bidirectional");
        config.mode = hs_sync_mode_from_string(mode_str);
        free(mode_str);

        char *cs_str = toml_string_or(general, "conflict_strategy", "newest_wins");
        config.conflict_strategy = hs_conflict_strategy_from_string(cs_str);
        free(cs_str);

        config.poll_interval = (int)toml_int_or(general, "poll_interval", 0);
        config.dry_run = toml_bool_or(general, "dry_run", 0);
        config.log_level = toml_string_or(general, "log_level", "info");
    } else {
        config.hostname = hs_get_hostname();
        config.log_level = strdup("info");
    }

    toml_table_t *git = toml_table_in(root, "git");
    if (git) {
        char *repo_str = toml_string_or(git, "repo", "~/.local/share/hyprsync");
        config.git.repo = hs_expand_path(repo_str);
        free(repo_str);
        config.git.auto_commit = toml_bool_or(git, "auto_commit", 1);
        config.git.commit_template = toml_string_or(git, "commit_template",
                                                     "hyprsync: update from $hostname");
    } else {
        config.git.repo = hs_expand_path("~/.local/share/hyprsync");
        config.git.auto_commit = 1;
        config.git.commit_template = strdup("hyprsync: update from $hostname");
    }

    toml_table_t *ssh = toml_table_in(root, "ssh");
    if (ssh) {
        char *key_str = toml_string_or(ssh, "key", "~/.ssh/id_ed25519");
        config.ssh.key = hs_expand_path(key_str);
        free(key_str);
        config.ssh.port = (int)toml_int_or(ssh, "port", 22);
        config.ssh.timeout = (int)toml_int_or(ssh, "timeout", 10);
    } else {
        config.ssh.key = hs_expand_path("~/.ssh/id_ed25519");
        config.ssh.port = 22;
        config.ssh.timeout = 10;
    }

    hs_vec_init(&config.devices);
    toml_array_t *devices = toml_array_in(root, "device");
    if (devices) {
        int ndev = toml_array_nelem(devices);
        for (int i = 0; i < ndev; i++) {
            toml_table_t *dev_tbl = toml_table_at(devices, i);
            if (!dev_tbl) continue;

            hs_device device;
            memset(&device, 0, sizeof(device));

            device.name = toml_string_or(dev_tbl, "name", "");
            device.host = toml_string_or(dev_tbl, "host", "");
            device.user = toml_string_or(dev_tbl, "user", "");
            device.port = (int)toml_int_or(dev_tbl, "port", config.ssh.port);

            toml_datum_t key_d = toml_string_in(dev_tbl, "key");
            if (key_d.ok && key_d.u.s[0] != '\0') {
                device.key = hs_expand_path(key_d.u.s);
                free(key_d.u.s);
            } else {
                device.key = strdup("");
                if (key_d.ok) free(key_d.u.s);
            }

            if (device.name[0] != '\0' && device.host[0] != '\0') {
                hs_vec_push(&config.devices, device);
            } else {
                hs_device_free(&device);
            }
        }
    }

    hs_vec_init(&config.sync_groups);
    toml_array_t *syncs = toml_array_in(root, "sync");
    if (syncs) {
        int nsync = toml_array_nelem(syncs);
        for (int i = 0; i < nsync; i++) {
            toml_table_t *sync_tbl = toml_table_at(syncs, i);
            if (!sync_tbl) continue;

            hs_sync_group group;
            memset(&group, 0, sizeof(group));
            group.name = toml_string_or(sync_tbl, "name", "");
            hs_vec_init(&group.paths);
            hs_vec_init(&group.exclude);
            hs_vec_init(&group.devices);

            toml_array_t *paths = toml_array_in(sync_tbl, "paths");
            if (paths) {
                int np = toml_array_nelem(paths);
                for (int j = 0; j < np; j++) {
                    toml_datum_t s = toml_string_at(paths, j);
                    if (s.ok) {
                        char *exp = hs_expand_path(s.u.s);
                        free(s.u.s);
                        hs_vec_push(&group.paths, exp);
                    }
                }
            }

            toml_array_t *excludes = toml_array_in(sync_tbl, "exclude");
            if (excludes) {
                int ne = toml_array_nelem(excludes);
                for (int j = 0; j < ne; j++) {
                    toml_datum_t s = toml_string_at(excludes, j);
                    if (s.ok)
                        hs_vec_push(&group.exclude, s.u.s);
                }
            }

            toml_array_t *devs = toml_array_in(sync_tbl, "devices");
            if (devs) {
                int nd = toml_array_nelem(devs);
                for (int j = 0; j < nd; j++) {
                    toml_datum_t s = toml_string_at(devs, j);
                    if (s.ok)
                        hs_vec_push(&group.devices, s.u.s);
                }
            }

            group.remote_path = toml_string_or(sync_tbl, "remote_path", "");

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

    toml_table_t *hooks = toml_table_in(root, "hooks");
    if (hooks) {
        toml_datum_t pre = toml_string_in(hooks, "pre_sync");
        if (pre.ok) {
            free(config.hooks.pre_sync);
            config.hooks.pre_sync = pre.u.s;
        }
        toml_datum_t post = toml_string_in(hooks, "post_sync");
        if (post.ok) {
            free(config.hooks.post_sync);
            config.hooks.post_sync = post.u.s;
        }

        toml_table_t *group_hooks = toml_table_in(hooks, "group");
        if (group_hooks) {
            for (int i = 0; ; i++) {
                const char *key = toml_key_in(group_hooks, i);
                if (!key) break;
                toml_datum_t val = toml_string_in(group_hooks, key);
                if (val.ok) {
                    hs_hook_entry entry;
                    entry.key = strdup(key);
                    entry.value = val.u.s;
                    hs_vec_push(&config.hooks.group_hooks, entry);
                }
            }
        }
    }

    toml_free(root);
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
