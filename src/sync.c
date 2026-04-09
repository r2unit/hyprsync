#define _POSIX_C_SOURCE 200809L

#include "sync.h"
#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *get_ssh_key(const hs_sync *s, const hs_device *device) {
    if (device->key && device->key[0])
        return strdup(device->key);
    return strdup(s->config->ssh.key);
}

static char *build_ssh_cmd(const hs_sync *s, const hs_device *device) {
    char *key = get_ssh_key(s, device);
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "ssh -i %s -p %d -o StrictHostKeyChecking=accept-new"
             " -o BatchMode=yes -o ConnectTimeout=%d -o LogLevel=ERROR",
             key, device->port, s->config->ssh.timeout);
    free(key);
    return strdup(buf);
}

static hs_exec_result remote_exec(const hs_sync *s, const hs_device *device,
                                  const char *command) {
    char *key = get_ssh_key(s, device);
    char port_str[16];
    char timeout_str[32];
    snprintf(port_str, sizeof(port_str), "%d", device->port);
    snprintf(timeout_str, sizeof(timeout_str), "ConnectTimeout=%d",
             s->config->ssh.timeout);

    char *userhost = NULL;
    {
        size_t len = strlen(device->user) + 1 + strlen(device->host) + 1;
        userhost = malloc(len);
        snprintf(userhost, len, "%s@%s", device->user, device->host);
    }

    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("ssh"));
    hs_vec_push(&args, strdup("-i"));
    hs_vec_push(&args, key);
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
    hs_vec_push(&args, userhost);
    hs_vec_push(&args, strdup(command));

    hs_exec_result result = hs_exec_args(&args);
    hs_strvec_free(&args);
    return result;
}

static int ensure_remote_dir(const hs_sync *s, const hs_device *device) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", s->config->git.repo);
    hs_exec_result res = remote_exec(s, device, cmd);
    int ok = hs_exec_success(&res);
    hs_exec_result_free(&res);
    return ok;
}

static hs_strvec build_rsync_cmd(const hs_sync *s, const hs_device *device,
                                 int dry_run) {
    char *ssh_cmd = build_ssh_cmd(s, device);

    hs_strvec cmd;
    hs_vec_init(&cmd);
    hs_vec_push(&cmd, strdup("rsync"));
    hs_vec_push(&cmd, strdup("-az"));
    hs_vec_push(&cmd, strdup("--checksum"));
    hs_vec_push(&cmd, strdup("--partial"));
    hs_vec_push(&cmd, strdup("--delete"));
    hs_vec_push(&cmd, strdup("--exclude=.git/"));
    hs_vec_push(&cmd, strdup("-e"));
    hs_vec_push(&cmd, ssh_cmd);

    if (dry_run)
        hs_vec_push(&cmd, strdup("--dry-run"));

    const char *repo = s->git->repo_path;
    size_t rlen = strlen(repo);
    if (rlen > 0 && repo[rlen - 1] != '/') {
        char *src = malloc(rlen + 2);
        memcpy(src, repo, rlen);
        src[rlen] = '/';
        src[rlen + 1] = '\0';
        hs_vec_push(&cmd, src);
    } else {
        hs_vec_push(&cmd, strdup(repo));
    }

    {
        size_t dlen = strlen(device->user) + 1 + strlen(device->host) + 1 +
                      strlen(s->config->git.repo) + 2;
        char *dest = malloc(dlen);
        snprintf(dest, dlen, "%s@%s:%s/", device->user, device->host,
                 s->config->git.repo);
        hs_vec_push(&cmd, dest);
    }

    return cmd;
}

hs_sync *hs_sync_create(const hs_config *config, hs_git *git) {
    hs_sync *s = calloc(1, sizeof(hs_sync));
    if (!s) return NULL;
    s->config = config;
    s->git = git;
    return s;
}

void hs_sync_free(hs_sync *s) {
    free(s);
}

hs_sync_result hs_sync_run_group(hs_sync *s, const hs_sync_group *group,
                                 const hs_device *device, int dry_run) {
    hs_sync_result result;
    memset(&result, 0, sizeof(result));
    result.device_name = hs_strdup_safe(device->name);
    result.group_name = hs_strdup_safe(group->name);
    result.success = 0;
    result.files_synced = 0;
    result.has_conflicts = 0;
    hs_vec_init(&result.conflict_files);

    if (group->devices.len > 0) {
        int device_allowed = 0;
        for (size_t i = 0; i < group->devices.len; i++) {
            if (strcmp(group->devices.data[i], device->name) == 0) {
                device_allowed = 1;
                break;
            }
        }
        if (!device_allowed) {
            result.success = 1;
            return result;
        }
    }

    if (!ensure_remote_dir(s, device)) {
        result.error_message = strdup("failed to create remote directory");
        return result;
    }

    hs_strvec rsync_cmd = build_rsync_cmd(s, device, dry_run);
    hs_exec_result exec_res = hs_exec_args(&rsync_cmd);
    hs_strvec_free(&rsync_cmd);

    if (hs_exec_success(&exec_res)) {
        result.success = 1;

        if (!dry_run) {
            hs_exec_result restore_res = remote_exec(s, device,
                "hyprsync restore 2>/dev/null || true");
            if (!hs_exec_success(&restore_res)) {
                hs_warn("remote restore may have failed on %s", device->name);
            }
            hs_exec_result_free(&restore_res);

            for (size_t i = 0; i < s->config->hooks.group_hooks.len; i++) {
                hs_hook_entry *h = &s->config->hooks.group_hooks.data[i];
                if (strcmp(h->key, group->name) == 0 && h->value && h->value[0]) {
                    hs_exec_result hook_res = remote_exec(s, device, h->value);
                    hs_exec_result_free(&hook_res);
                    break;
                }
            }
        }
    } else {
        result.error_message = hs_strdup_safe(exec_res.stderr_output);
    }

    hs_exec_result_free(&exec_res);
    return result;
}

hs_sync_resultvec hs_sync_all(hs_sync *s, int dry_run) {
    hs_sync_resultvec results;
    hs_vec_init(&results);

    if (hs_git_has_conflicts(s->git)) {
        hs_warn("existing conflicts detected - resolve with 'hyprsync conflicts resolve' before syncing");
        hs_sync_result conflict_result;
        memset(&conflict_result, 0, sizeof(conflict_result));
        conflict_result.success = 0;
        conflict_result.has_conflicts = 1;
        conflict_result.error_message = strdup("unresolved conflicts exist");
        hs_vec_init(&conflict_result.conflict_files);

        hs_conflictvec conflicts = hs_git_get_conflicts(s->git);
        for (size_t i = 0; i < conflicts.len; i++) {
            hs_vec_push(&conflict_result.conflict_files,
                        strdup(conflicts.data[i].path));
        }
        hs_conflictvec_free(&conflicts);

        hs_vec_push(&results, conflict_result);
        return results;
    }

    if (s->config->hooks.pre_sync && s->config->hooks.pre_sync[0]) {
        hs_exec_result pre = hs_exec(s->config->hooks.pre_sync);
        hs_exec_result_free(&pre);
    }

    hs_git_snapshot(s->git, &s->config->sync_groups);

    if (s->config->git.auto_commit && hs_git_has_changes(s->git)) {
        char *message = strdup(s->config->git.commit_template
                               ? s->config->git.commit_template : "");
        char *pos = strstr(message, "$hostname");
        if (pos) {
            const char *hn = s->config->hostname ? s->config->hostname : "";
            size_t prefix_len = (size_t)(pos - message);
            size_t hn_len = strlen(hn);
            size_t suffix_len = strlen(pos + 9);
            size_t new_len = prefix_len + hn_len + suffix_len + 1;
            char *new_msg = malloc(new_len);
            memcpy(new_msg, message, prefix_len);
            memcpy(new_msg + prefix_len, hn, hn_len);
            memcpy(new_msg + prefix_len + hn_len, pos + 9, suffix_len);
            new_msg[new_len - 1] = '\0';
            free(message);
            message = new_msg;
        }
        hs_git_commit(s->git, message);
        free(message);
    }

    for (size_t d = 0; d < s->config->devices.len; d++) {
        const hs_device *device = &s->config->devices.data[d];
        for (size_t g = 0; g < s->config->sync_groups.len; g++) {
            const hs_sync_group *group = &s->config->sync_groups.data[g];
            hs_sync_result r = hs_sync_run_group(s, group, device, dry_run);
            hs_vec_push(&results, r);
        }
    }

    if (s->config->hooks.post_sync && s->config->hooks.post_sync[0]) {
        hs_exec_result post = hs_exec(s->config->hooks.post_sync);
        hs_exec_result_free(&post);
    }

    return results;
}

hs_diff_result hs_sync_diff(hs_sync *s, const hs_device *device) {
    hs_diff_result result;
    memset(&result, 0, sizeof(result));
    result.device_name = hs_strdup_safe(device->name);

    hs_strvec local_files = hs_git_changed_files(s->git);
    result.local_changes = local_files;
    hs_vec_init(&result.remote_changes);

    return result;
}

int hs_sync_ping(hs_sync *s, const hs_device *device) {
    char *key = get_ssh_key(s, device);
    char port_str[16];
    char timeout_str[32];
    snprintf(port_str, sizeof(port_str), "%d", device->port);
    snprintf(timeout_str, sizeof(timeout_str), "ConnectTimeout=%d",
             s->config->ssh.timeout);

    size_t uh_len = strlen(device->user) + 1 + strlen(device->host) + 1;
    char *userhost = malloc(uh_len);
    snprintf(userhost, uh_len, "%s@%s", device->user, device->host);

    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("ssh"));
    hs_vec_push(&args, strdup("-i"));
    hs_vec_push(&args, key);
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

void hs_sync_result_free(hs_sync_result *r) {
    if (!r) return;
    free(r->device_name);
    free(r->group_name);
    free(r->error_message);
    hs_strvec_free(&r->conflict_files);
    r->device_name = NULL;
    r->group_name = NULL;
    r->error_message = NULL;
}

void hs_sync_resultvec_free(hs_sync_resultvec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++)
        hs_sync_result_free(&v->data[i]);
    hs_vec_free(v);
}

void hs_diff_result_free(hs_diff_result *r) {
    if (!r) return;
    free(r->device_name);
    hs_strvec_free(&r->local_changes);
    hs_strvec_free(&r->remote_changes);
    r->device_name = NULL;
}
