#ifndef HS_GIT_H
#define HS_GIT_H

#include "config.h"
#include "util.h"

typedef struct {
    char *path;
    char *ours;
    char *theirs;
} hs_conflict_file;

typedef hs_vec(hs_conflict_file) hs_conflictvec;

typedef struct {
    hs_git_config config;
    char *hostname;
    char *repo_path;
    char *home_dir;
} hs_git;

hs_git *hs_git_create(const hs_git_config *config, const char *hostname);
void hs_git_free(hs_git *g);

int hs_git_init_repo(hs_git *g);
int hs_git_is_initialized(const hs_git *g);

void hs_git_snapshot(hs_git *g, const hs_groupvec *groups);
void hs_git_snapshot_changed(hs_git *g, const hs_strvec *changed_paths,
                             const hs_groupvec *groups);
void hs_git_restore(hs_git *g, const hs_groupvec *groups);

int hs_git_commit(hs_git *g, const char *message);
int hs_git_has_changes(const hs_git *g);

char *hs_git_diff(const hs_git *g);
char *hs_git_diff_staged(const hs_git *g);
char *hs_git_diff_remote(const hs_git *g, const char *device);

hs_strvec hs_git_log(const hs_git *g, int count);
hs_strvec hs_git_changed_files(const hs_git *g);

hs_conflictvec hs_git_get_conflicts(const hs_git *g);
int hs_git_resolve_conflict(hs_git *g, const char *file,
                            hs_conflict_strategy strategy);
int hs_git_has_conflicts(const hs_git *g);

void hs_git_create_device_branch(hs_git *g, const char *device);
void hs_git_update_device_branch(hs_git *g, const char *device);

char *hs_git_to_repo_path(const hs_git *g, const char *original);
char *hs_git_to_original_path(const hs_git *g, const char *repo_relative);

void hs_conflict_file_free(hs_conflict_file *f);
void hs_conflictvec_free(hs_conflictvec *v);

#endif
