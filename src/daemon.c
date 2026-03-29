#include "daemon.h"
#include "log.h"
#include "util.h"

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

static atomic_bool *g_running = NULL;
static atomic_bool g_reload_requested = false;

static void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (g_running) {
            atomic_store(g_running, false);
        }
    } else if (sig == SIGHUP) {
        atomic_store(&g_reload_requested, true);
    }
}

static void setup_signal_handlers(hs_daemon *d) {
    g_running = &d->running;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

static void handle_changes(hs_daemon *d, hs_eventvec *events) {
    hs_debug("handling %zu file events", events->len);

    int needs_full_snapshot = 0;
    hs_strvec changed_paths;
    hs_vec_init(&changed_paths);

    for (size_t i = 0; i < events->len; i++) {
        if (strcmp(events->data[i].path, "/") == 0) {
            needs_full_snapshot = 1;
            break;
        }
        hs_vec_push(&changed_paths, strdup(events->data[i].path));
    }

    if (needs_full_snapshot) {
        hs_info("running full snapshot after overflow");
        hs_git_snapshot(d->git, &d->config.sync_groups);
    } else {
        hs_git_snapshot_changed(d->git, &changed_paths, &d->config.sync_groups);
    }

    hs_strvec_free(&changed_paths);

    if (!hs_git_has_changes(d->git)) {
        hs_debug("no actual changes after snapshot");
        return;
    }

    char *message = strdup(d->config.git.commit_template);
    char *pos = strstr(message, "$hostname");
    if (pos) {
        size_t prefix_len = (size_t)(pos - message);
        size_t hostname_len = strlen(d->config.hostname);
        size_t suffix_len = strlen(pos + 9);
        size_t new_len = prefix_len + hostname_len + suffix_len + 1;
        char *new_message = malloc(new_len);
        memcpy(new_message, message, prefix_len);
        memcpy(new_message + prefix_len, d->config.hostname, hostname_len);
        memcpy(new_message + prefix_len + hostname_len, pos + 9, suffix_len + 1);
        free(message);
        message = new_message;
    }

    if (d->config.git.auto_commit) {
        hs_git_commit(d->git, message);
    }

    free(message);

    if (!d->config.dry_run) {
        hs_sync_resultvec results = hs_sync_all(d->sync, 0);
        for (size_t i = 0; i < results.len; i++) {
            hs_sync_result *r = &results.data[i];
            if (r->success) {
                hs_info("synced %s to %s", r->group_name, r->device_name);
            } else {
                hs_error("failed to sync %s to %s: %s",
                         r->group_name, r->device_name, r->error_message);
            }
        }
        hs_sync_resultvec_free(&results);
    }
}

static void reload_config(hs_daemon *d) {
    hs_info("reloading configuration");

    char *path = hs_default_config_path();
    if (!path) {
        hs_error("failed to get default config path");
        return;
    }

    hs_config new_config = hs_load_config(path);
    free(path);

    if (!new_config.hostname) {
        hs_error("failed to reload config");
        return;
    }

    hs_watcher_stop(d->watcher);
    hs_watcher_free(d->watcher);
    hs_sync_free(d->sync);
    hs_git_free(d->git);
    hs_config_free(&d->config);

    d->config = new_config;
    d->git = hs_git_create(&d->config.git, d->config.hostname);
    d->sync = hs_sync_create(&d->config, d->git);
    d->watcher = hs_watcher_create(&d->config);
    hs_watcher_start(d->watcher);

    hs_info("configuration reloaded");
}

hs_daemon *hs_daemon_create(hs_config config) {
    hs_daemon *d = calloc(1, sizeof(hs_daemon));
    d->config = config;
    d->watcher = NULL;
    d->sync = NULL;
    d->git = NULL;
    atomic_init(&d->running, false);
    return d;
}

void hs_daemon_free(hs_daemon *d) {
    if (!d) return;
    if (d->watcher) hs_watcher_free(d->watcher);
    if (d->sync) hs_sync_free(d->sync);
    if (d->git) hs_git_free(d->git);
    hs_config_free(&d->config);
    free(d);
}

void hs_daemon_run(hs_daemon *d) {
    d->git = hs_git_create(&d->config.git, d->config.hostname);
    d->sync = hs_sync_create(&d->config, d->git);
    d->watcher = hs_watcher_create(&d->config);

    setup_signal_handlers(d);

    if (!hs_git_is_initialized(d->git)) {
        hs_error("git repo not initialized");
        return;
    }

    hs_info("starting hyprsync daemon");
    hs_info("hostname: %s", d->config.hostname);
    hs_info("watching %zu sync groups", d->config.sync_groups.len);

#ifdef WITH_SYSTEMD
    sd_notify(0, "READY=1");
#endif

    atomic_store(&d->running, true);
    hs_watcher_start(d->watcher);

    while (atomic_load(&d->running)) {
        if (atomic_load(&g_reload_requested)) {
            atomic_store(&g_reload_requested, false);
            reload_config(d);
        }

        hs_eventvec events = hs_watcher_poll(d->watcher, 1000);

        if (events.len > 0) {
            handle_changes(d, &events);
        }

        hs_eventvec_free(&events);

#ifdef WITH_SYSTEMD
        sd_notify(0, "WATCHDOG=1");
#endif
    }

    hs_watcher_stop(d->watcher);

#ifdef WITH_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    hs_info("hyprsync daemon stopped");
}

void hs_daemon_shutdown(hs_daemon *d) {
    atomic_store(&d->running, false);
}
