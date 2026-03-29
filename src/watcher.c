#define _POSIX_C_SOURCE 200809L

#include "watcher.h"
#include "util.h"
#include "log.h"

#include <stdbool.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define EVENT_BUF_LEN 4096
#define DEBOUNCE_MS 500

static void add_watch_recursive(hs_watcher *w, const char *path);
static void remove_watch(hs_watcher *w, int wd);
static void rescan(hs_watcher *w);
static void process_event(hs_watcher *w, int wd, uint32_t mask, const char *name);
static void *watch_loop(void *arg);

static const char *basename_ptr(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

hs_watcher *hs_watcher_create(const hs_config *config) {
    hs_watcher *w = calloc(1, sizeof(hs_watcher));
    if (!w) return NULL;

    w->config = config;
    w->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (w->inotify_fd < 0) {
        hs_error("failed to initialize inotify: %s", strerror(errno));
    }

    hs_vec_init(&w->watch_descriptors);
    hs_vec_init(&w->pending_events);
    hs_vec_init(&w->seen_paths);

    pthread_mutex_init(&w->queue_mutex, NULL);
    pthread_cond_init(&w->queue_cv, NULL);

    atomic_store(&w->running, false);

    return w;
}

void hs_watcher_free(hs_watcher *w) {
    if (!w) return;

    hs_watcher_stop(w);

    if (w->inotify_fd >= 0) {
        close(w->inotify_fd);
    }

    pthread_mutex_destroy(&w->queue_mutex);
    pthread_cond_destroy(&w->queue_cv);

    for (size_t i = 0; i < w->watch_descriptors.len; i++) {
        free(w->watch_descriptors.data[i].path);
    }
    hs_vec_free(&w->watch_descriptors);

    for (size_t i = 0; i < w->pending_events.len; i++) {
        hs_file_event_free(&w->pending_events.data[i]);
    }
    hs_vec_free(&w->pending_events);

    hs_strvec_free(&w->seen_paths);

    free(w);
}

void hs_watcher_start(hs_watcher *w) {
    if (atomic_load(&w->running) || w->inotify_fd < 0) {
        return;
    }

    atomic_store(&w->running, true);

    for (size_t g = 0; g < w->config->sync_groups.len; g++) {
        hs_sync_group *group = &w->config->sync_groups.data[g];
        for (size_t p = 0; p < group->paths.len; p++) {
            char *expanded = hs_expand_path(group->paths.data[p]);
            if (hs_dir_exists(expanded) || hs_file_exists(expanded)) {
                add_watch_recursive(w, expanded);
            }
            free(expanded);
        }
    }

    pthread_create(&w->watch_thread, NULL, watch_loop, w);

    hs_info("watcher started with %zu watches", w->watch_descriptors.len);
}

void hs_watcher_stop(hs_watcher *w) {
    if (!atomic_load(&w->running)) {
        return;
    }

    atomic_store(&w->running, false);

    pthread_cond_broadcast(&w->queue_cv);

    pthread_join(w->watch_thread, NULL);

    for (size_t i = 0; i < w->watch_descriptors.len; i++) {
        inotify_rm_watch(w->inotify_fd, w->watch_descriptors.data[i].wd);
        free(w->watch_descriptors.data[i].path);
    }
    hs_vec_clear(&w->watch_descriptors);
    w->watch_descriptors.len = 0;

    hs_debug("watcher stopped");
}

hs_eventvec hs_watcher_poll(hs_watcher *w, int timeout_ms) {
    pthread_mutex_lock(&w->queue_mutex);

    if (w->pending_events.len == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_cond_timedwait(&w->queue_cv, &w->queue_mutex, &ts);
    }

    hs_eventvec events;
    hs_vec_init(&events);

    hs_eventvec tmp = w->pending_events;
    w->pending_events = events;
    events = tmp;

    hs_strvec_free(&w->seen_paths);
    hs_vec_init(&w->seen_paths);

    pthread_mutex_unlock(&w->queue_mutex);

    return events;
}

static char *find_path_by_wd(hs_watcher *w, int wd) {
    for (size_t i = 0; i < w->watch_descriptors.len; i++) {
        if (w->watch_descriptors.data[i].wd == wd) {
            return w->watch_descriptors.data[i].path;
        }
    }
    return NULL;
}

static void add_watch_recursive(hs_watcher *w, const char *path) {
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    if (hs_dir_exists(path)) {
        int wd = inotify_add_watch(w->inotify_fd, path, mask);
        if (wd >= 0) {
            hs_watch_entry entry;
            entry.wd = wd;
            entry.path = strdup(path);
            hs_vec_push(&w->watch_descriptors, entry);
            hs_debug("watching directory: %s", path);
        } else {
            hs_warn("failed to watch %s: %s", path, strerror(errno));
        }

        DIR *dir = opendir(path);
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                if (strcmp(ent->d_name, ".git") == 0) continue;

                char *child = hs_join_path(path, ent->d_name);
                if (hs_dir_exists(child)) {
                    add_watch_recursive(w, child);
                }
                free(child);
            }
            closedir(dir);
        }
    } else if (hs_file_exists(path)) {
        char *parent = strdup(path);
        char *slash = strrchr(parent, '/');
        if (slash) {
            *slash = '\0';
        }
        int wd = inotify_add_watch(w->inotify_fd, parent, mask);
        if (wd >= 0) {
            hs_watch_entry entry;
            entry.wd = wd;
            entry.path = parent;
            hs_vec_push(&w->watch_descriptors, entry);
            hs_debug("watching file's parent: %s", parent);
        } else {
            free(parent);
        }
    }
}

static void remove_watch(hs_watcher *w, int wd) {
    for (size_t i = 0; i < w->watch_descriptors.len; i++) {
        if (w->watch_descriptors.data[i].wd == wd) {
            free(w->watch_descriptors.data[i].path);
            hs_vec_remove(&w->watch_descriptors, i);
            return;
        }
    }
}

static void rescan(hs_watcher *w) {
    for (size_t i = 0; i < w->watch_descriptors.len; i++) {
        inotify_rm_watch(w->inotify_fd, w->watch_descriptors.data[i].wd);
        free(w->watch_descriptors.data[i].path);
    }
    hs_vec_clear(&w->watch_descriptors);

    for (size_t g = 0; g < w->config->sync_groups.len; g++) {
        hs_sync_group *group = &w->config->sync_groups.data[g];
        for (size_t p = 0; p < group->paths.len; p++) {
            char *expanded = hs_expand_path(group->paths.data[p]);
            if (hs_dir_exists(expanded) || hs_file_exists(expanded)) {
                add_watch_recursive(w, expanded);
            }
            free(expanded);
        }
    }

    hs_info("rescan complete, %zu watches active", w->watch_descriptors.len);
}

static void process_event(hs_watcher *w, int wd, uint32_t mask, const char *name) {
    char *dir_path = find_path_by_wd(w, wd);
    if (!dir_path) return;

    char *full_path;
    if (name && strlen(name) > 0) {
        full_path = hs_join_path(dir_path, name);
    } else {
        full_path = strdup(dir_path);
    }

    const char *filename = basename_ptr(full_path);
    if (strncmp(filename, ".git", 4) == 0) {
        free(full_path);
        return;
    }

    hs_file_event event;
    event.path = full_path;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);

    if (mask & IN_CREATE) {
        event.type = HS_EVENT_CREATED;
        hs_debug("file created: %s", full_path);

        if (hs_dir_exists(full_path)) {
            add_watch_recursive(w, full_path);
        }
    } else if (mask & IN_MODIFY) {
        event.type = HS_EVENT_MODIFIED;
        hs_debug("file modified: %s", full_path);
    } else if (mask & IN_DELETE) {
        event.type = HS_EVENT_DELETED;
        hs_debug("file deleted: %s", full_path);
    } else if (mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
        event.type = HS_EVENT_MOVED;
        hs_debug("file moved: %s", full_path);
    } else {
        free(full_path);
        return;
    }

    if (mask & IN_IGNORED) {
        remove_watch(w, wd);
    }

    pthread_mutex_lock(&w->queue_mutex);

    if (hs_strvec_contains(&w->seen_paths, full_path)) {
        pthread_mutex_unlock(&w->queue_mutex);
        free(full_path);
        return;
    }

    hs_vec_push(&w->seen_paths, strdup(full_path));
    hs_vec_push(&w->pending_events, event);

    pthread_mutex_unlock(&w->queue_mutex);
}

static void *watch_loop(void *arg) {
    hs_watcher *w = (hs_watcher *)arg;
    char buffer[EVENT_BUF_LEN];
    struct timespec last_event_time;
    clock_gettime(CLOCK_MONOTONIC, &last_event_time);

    while (atomic_load(&w->running)) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(w->inotify_fd, &fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(w->inotify_fd + 1, &fds, NULL, NULL, &tv);

        if (ret < 0) {
            if (errno != EINTR) {
                hs_error("select error: %s", strerror(errno));
            }
            continue;
        }

        if (ret == 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed_ms = (now.tv_sec - last_event_time.tv_sec) * 1000 +
                              (now.tv_nsec - last_event_time.tv_nsec) / 1000000;

            pthread_mutex_lock(&w->queue_mutex);
            int has_events = w->pending_events.len > 0;
            pthread_mutex_unlock(&w->queue_mutex);

            if (has_events && elapsed_ms > DEBOUNCE_MS) {
                pthread_cond_signal(&w->queue_cv);
            }
            continue;
        }

        ssize_t len = read(w->inotify_fd, buffer, EVENT_BUF_LEN);
        if (len < 0) {
            if (errno != EAGAIN) {
                hs_error("read error: %s", strerror(errno));
            }
            continue;
        }

        ssize_t i = 0;
        while (i < len) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->mask & IN_Q_OVERFLOW) {
                hs_warn("inotify queue overflow, triggering full rescan");
                rescan(w);

                hs_file_event overflow_event;
                overflow_event.type = HS_EVENT_MODIFIED;
                overflow_event.path = strdup("/");
                clock_gettime(CLOCK_MONOTONIC, &overflow_event.timestamp);

                pthread_mutex_lock(&w->queue_mutex);
                for (size_t j = 0; j < w->pending_events.len; j++) {
                    hs_file_event_free(&w->pending_events.data[j]);
                }
                hs_vec_clear(&w->pending_events);
                hs_strvec_free(&w->seen_paths);
                hs_vec_init(&w->seen_paths);
                hs_vec_push(&w->pending_events, overflow_event);
                pthread_mutex_unlock(&w->queue_mutex);
                pthread_cond_signal(&w->queue_cv);
            } else {
                const char *name = event->len > 0 ? event->name : "";
                process_event(w, event->wd, event->mask, name);
                clock_gettime(CLOCK_MONOTONIC, &last_event_time);
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    return NULL;
}

void hs_file_event_free(hs_file_event *e) {
    if (!e) return;
    free(e->path);
    e->path = NULL;
}

void hs_eventvec_free(hs_eventvec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) {
        hs_file_event_free(&v->data[i]);
    }
    hs_vec_free(v);
}
