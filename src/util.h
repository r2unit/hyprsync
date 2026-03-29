#ifndef HS_UTIL_H
#define HS_UTIL_H

#include "vec.h"

typedef struct {
    int exit_code;
    char *stdout_output;
    char *stderr_output;
} hs_exec_result;

static inline int hs_exec_success(const hs_exec_result *r) {
    return r->exit_code == 0;
}

void hs_exec_result_free(hs_exec_result *r);

char *hs_expand_path(const char *path);

char *hs_get_env(const char *name);

char *hs_get_home_dir(void);

char *hs_get_hostname(void);

hs_exec_result hs_exec(const char *command);

hs_exec_result hs_exec_args(const hs_strvec *args);

hs_exec_result hs_exec_args_dir(const hs_strvec *args, const char *workdir);

char *hs_trim(const char *str);

hs_strvec hs_split(const char *str, char delimiter);

int hs_file_exists(const char *path);

int hs_dir_exists(const char *path);

char *hs_join_path(const char *base, const char *rel);

char *hs_strdup_safe(const char *s);

#endif
