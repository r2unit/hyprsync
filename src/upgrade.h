#ifndef HS_UPGRADE_H
#define HS_UPGRADE_H

#include "vec.h"

typedef struct {
    int year;
    int month;
    int number;
} hs_version;

typedef struct {
    char *tag_name;
    hs_version version;
    char *name;
    char *body;
    char *published_at;
    char *download_url;
    int prerelease;
} hs_release;

typedef hs_vec(hs_release) hs_releasevec;

typedef enum {
    HS_INSTALL_SCRIPT,
    HS_INSTALL_PACKAGE_MANAGER,
    HS_INSTALL_UNKNOWN
} hs_install_method;

char *hs_version_to_string(hs_version v);
int hs_version_parse(const char *str, hs_version *out);
int hs_version_cmp(hs_version a, hs_version b);
int hs_version_eq(hs_version a, hs_version b);

hs_version hs_current_version(void);
hs_releasevec hs_fetch_releases(void);

int hs_get_latest_release(hs_release *out);
int hs_get_latest_dev_release(hs_release *out);
int hs_get_release_by_version(const char *version, hs_release *out);

int hs_has_update(void);

hs_install_method hs_detect_install_method(void);

int hs_upgrade(const hs_release *release);
int hs_upgrade_to_latest(void);
int hs_upgrade_to_latest_dev(void);
int hs_upgrade_to_version(const char *version);

void hs_list_available_versions(void);

char *hs_get_binary_path(void);

void hs_release_free(hs_release *r);
void hs_releasevec_free(hs_releasevec *v);

const char *hs_install_method_to_string(hs_install_method method);

#endif
