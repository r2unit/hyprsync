#ifndef HS_WATCHER_H
#define HS_WATCHER_H

#include "config.h"

#include <stdatomic.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    HS_EVENT_CREATED,
    HS_EVENT_MODIFIED,
    HS_EVENT_DELETED,
    HS_EVENT_MOVED
} hs_file_event_type;

typedef struct {
    hs_file_event_type type;
    char *path;
    struct timespec timestamp;
} hs_file_event;

typedef hs_vec(hs_file_event) hs_eventvec;

typedef struct {
    int wd;
    char *path;
} hs_watch_entry;

typedef hs_vec(hs_watch_entry) hs_watchvec;

typedef struct {
    const hs_config *config;
    int inotify_fd;
    hs_watchvec watch_descriptors;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cv;
    hs_eventvec pending_events;
    hs_strvec seen_paths;
    atomic_bool running;
    pthread_t watch_thread;
} hs_watcher;

hs_watcher *hs_watcher_create(const hs_config *config);
void hs_watcher_free(hs_watcher *w);

void hs_watcher_start(hs_watcher *w);
void hs_watcher_stop(hs_watcher *w);

hs_eventvec hs_watcher_poll(hs_watcher *w, int timeout_ms);

void hs_file_event_free(hs_file_event *e);
void hs_eventvec_free(hs_eventvec *v);

#endif
