#ifndef HS_CONFIG_H
#define HS_CONFIG_H

#include "vec.h"

typedef enum {
    HS_SYNC_PUSH,
    HS_SYNC_PULL,
    HS_SYNC_BIDIRECTIONAL
} hs_sync_mode;

typedef enum {
    HS_CONFLICT_NEWEST_WINS,
    HS_CONFLICT_MANUAL,
    HS_CONFLICT_KEEP_BOTH
} hs_conflict_strategy;

typedef struct {
    char *name;
    char *host;
    char *user;
    int port;
    char *key;
} hs_device;

typedef struct {
    char *name;
    hs_strvec paths;
    hs_strvec exclude;
    hs_strvec devices;
    char *remote_path;
} hs_sync_group;

typedef struct {
    char *repo;
    int auto_commit;
    char *commit_template;
} hs_git_config;

typedef struct {
    char *key;
    int port;
    int timeout;
} hs_ssh_config;

typedef struct {
    char *key;
    char *value;
} hs_hook_entry;

typedef hs_vec(hs_hook_entry) hs_hookvec;

typedef struct {
    char *pre_sync;
    char *post_sync;
    hs_hookvec group_hooks;
} hs_hooks_config;

typedef hs_vec(hs_device) hs_devicevec;
typedef hs_vec(hs_sync_group) hs_groupvec;

typedef struct {
    char *hostname;
    hs_sync_mode mode;
    hs_conflict_strategy conflict_strategy;
    int poll_interval;
    int dry_run;
    char *log_level;

    hs_git_config git;
    hs_ssh_config ssh;

    hs_devicevec devices;
    hs_groupvec sync_groups;

    hs_hooks_config hooks;
} hs_config;

const char *hs_sync_mode_to_string(hs_sync_mode mode);
hs_sync_mode hs_sync_mode_from_string(const char *str);

const char *hs_conflict_strategy_to_string(hs_conflict_strategy strategy);
hs_conflict_strategy hs_conflict_strategy_from_string(const char *str);

hs_config hs_load_config(const char *path);
void hs_save_config(const hs_config *config, const char *path);
void hs_config_free(hs_config *config);

char *hs_default_config_path(void);
char *hs_default_repo_path(void);

void hs_device_free(hs_device *d);
void hs_sync_group_free(hs_sync_group *g);

#endif
