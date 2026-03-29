#ifndef HS_SYNC_H
#define HS_SYNC_H

#include "config.h"
#include "git.h"

typedef struct {
    int success;
    char *device_name;
    char *group_name;
    int files_synced;
    char *error_message;
    int has_conflicts;
    hs_strvec conflict_files;
} hs_sync_result;

typedef struct {
    char *device_name;
    hs_strvec local_changes;
    hs_strvec remote_changes;
} hs_diff_result;

typedef hs_vec(hs_sync_result) hs_sync_resultvec;

typedef struct {
    const hs_config *config;
    hs_git *git;
} hs_sync;

hs_sync *hs_sync_create(const hs_config *config, hs_git *git);
void hs_sync_free(hs_sync *s);

hs_sync_result hs_sync_run_group(hs_sync *s, const hs_sync_group *group,
                                const hs_device *device, int dry_run);
hs_sync_resultvec hs_sync_all(hs_sync *s, int dry_run);

hs_diff_result hs_sync_diff(hs_sync *s, const hs_device *device);

int hs_sync_ping(hs_sync *s, const hs_device *device);

void hs_sync_result_free(hs_sync_result *r);
void hs_sync_resultvec_free(hs_sync_resultvec *v);
void hs_diff_result_free(hs_diff_result *r);

#endif
