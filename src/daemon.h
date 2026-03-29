#ifndef HS_DAEMON_H
#define HS_DAEMON_H

#include "config.h"
#include "git.h"
#include "watcher.h"
#include "sync.h"

#include <stdatomic.h>

typedef struct {
    hs_config config;
    hs_watcher *watcher;
    hs_sync *sync;
    hs_git *git;
    atomic_bool running;
} hs_daemon;

hs_daemon *hs_daemon_create(hs_config config);
void hs_daemon_free(hs_daemon *d);

void hs_daemon_run(hs_daemon *d);
void hs_daemon_shutdown(hs_daemon *d);

#endif
