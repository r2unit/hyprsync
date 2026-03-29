#include "upgrade.h"
#include "util.h"
#include "log.h"

#include <hyprsync/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char *hs_version_to_string(hs_version v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d.%d.%d", v.year, v.month, v.number);
    return strdup(buf);
}

int hs_version_parse(const char *str, hs_version *out) {
    if (!str || !out) return 0;

    const char *p = str;
    if (*p == 'v') p++;

    int y, m, n;
    char trail = 0;
    int matched = sscanf(p, "%d.%d.%d%c", &y, &m, &n, &trail);
    if (matched < 3) return 0;

    if (matched == 4 && trail != '-') return 0;

    out->year = y;
    out->month = m;
    out->number = n;
    return 1;
}

int hs_version_cmp(hs_version a, hs_version b) {
    if (a.year != b.year) return a.year - b.year;
    if (a.month != b.month) return a.month - b.month;
    return a.number - b.number;
}

int hs_version_eq(hs_version a, hs_version b) {
    return a.year == b.year && a.month == b.month && a.number == b.number;
}

hs_version hs_current_version(void) {
    hs_version v;
    v.year = HS_VERSION_YEAR;
    v.month = HS_VERSION_MONTH;
    v.number = HS_VERSION_NUMBER;
    return v;
}

char *hs_get_binary_path(void) {
    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("readlink"));
    hs_vec_push(&args, strdup("-f"));
    hs_vec_push(&args, strdup("/proc/self/exe"));

    hs_exec_result res = hs_exec_args(&args);
    hs_strvec_free(&args);

    if (hs_exec_success(&res)) {
        char *path = hs_trim(res.stdout_output);
        hs_exec_result_free(&res);
        return path;
    }
    hs_exec_result_free(&res);

    hs_vec_init(&args);
    hs_vec_push(&args, strdup("which"));
    hs_vec_push(&args, strdup("hyprsync"));

    res = hs_exec_args(&args);
    hs_strvec_free(&args);

    if (hs_exec_success(&res)) {
        char *path = hs_trim(res.stdout_output);
        hs_exec_result_free(&res);
        return path;
    }
    hs_exec_result_free(&res);

    return hs_expand_path("~/.local/bin/hyprsync");
}

static int read_install_marker(void) {
    char *marker_dir = hs_expand_path("~/.local/share/hyprsync");
    char *marker_path = hs_join_path(marker_dir, HS_INSTALL_MARKER_FILE);
    free(marker_dir);

    if (!hs_file_exists(marker_path)) {
        free(marker_path);
        return HS_INSTALL_UNKNOWN;
    }

    FILE *f = fopen(marker_path, "r");
    free(marker_path);
    if (!f) return HS_INSTALL_UNKNOWN;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return HS_INSTALL_UNKNOWN;
    }
    fclose(f);

    char *trimmed = hs_trim(line);
    hs_install_method method = HS_INSTALL_UNKNOWN;
    if (strcmp(trimmed, "script") == 0) method = HS_INSTALL_SCRIPT;
    else if (strcmp(trimmed, "package") == 0) method = HS_INSTALL_PACKAGE_MANAGER;
    free(trimmed);

    return method;
}

static void write_install_marker(hs_install_method method) {
    char *marker_dir = hs_expand_path("~/.local/share/hyprsync");

    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("mkdir"));
    hs_vec_push(&args, strdup("-p"));
    hs_vec_push(&args, strdup(marker_dir));
    hs_exec_result res = hs_exec_args(&args);
    hs_strvec_free(&args);
    hs_exec_result_free(&res);

    char *marker_path = hs_join_path(marker_dir, HS_INSTALL_MARKER_FILE);
    free(marker_dir);

    FILE *f = fopen(marker_path, "w");
    free(marker_path);
    if (!f) return;

    fprintf(f, "%s\n", hs_install_method_to_string(method));
    fclose(f);
}

hs_install_method hs_detect_install_method(void) {
    hs_install_method marker = read_install_marker();
    if (marker != HS_INSTALL_UNKNOWN) return marker;

    char *binary_path = hs_get_binary_path();
    if (!binary_path) return HS_INSTALL_UNKNOWN;

    if (strncmp(binary_path, "/usr/bin", 8) == 0 ||
        strncmp(binary_path, "/usr/local/bin", 14) == 0) {

        hs_strvec args;

        hs_vec_init(&args);
        hs_vec_push(&args, strdup("pacman"));
        hs_vec_push(&args, strdup("-Qo"));
        hs_vec_push(&args, strdup(binary_path));
        hs_exec_result res = hs_exec_args(&args);
        hs_strvec_free(&args);
        if (hs_exec_success(&res)) {
            hs_exec_result_free(&res);
            free(binary_path);
            return HS_INSTALL_PACKAGE_MANAGER;
        }
        hs_exec_result_free(&res);

        hs_vec_init(&args);
        hs_vec_push(&args, strdup("dpkg"));
        hs_vec_push(&args, strdup("-S"));
        hs_vec_push(&args, strdup(binary_path));
        res = hs_exec_args(&args);
        hs_strvec_free(&args);
        if (hs_exec_success(&res)) {
            hs_exec_result_free(&res);
            free(binary_path);
            return HS_INSTALL_PACKAGE_MANAGER;
        }
        hs_exec_result_free(&res);

        hs_vec_init(&args);
        hs_vec_push(&args, strdup("rpm"));
        hs_vec_push(&args, strdup("-qf"));
        hs_vec_push(&args, strdup(binary_path));
        res = hs_exec_args(&args);
        hs_strvec_free(&args);
        if (hs_exec_success(&res)) {
            hs_exec_result_free(&res);
            free(binary_path);
            return HS_INSTALL_PACKAGE_MANAGER;
        }
        hs_exec_result_free(&res);
    }

    if (strstr(binary_path, "/.local/bin") != NULL) {
        free(binary_path);
        return HS_INSTALL_SCRIPT;
    }

    free(binary_path);
    return HS_INSTALL_UNKNOWN;
}

static char *fetch_github_api(const char *url) {
    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("curl"));
    hs_vec_push(&args, strdup("-s"));
    hs_vec_push(&args, strdup("-L"));
    hs_vec_push(&args, strdup("-H"));
    hs_vec_push(&args, strdup("Accept: application/vnd.github+json"));
    hs_vec_push(&args, strdup("-H"));
    hs_vec_push(&args, strdup("X-GitHub-Api-Version: 2022-11-28"));
    hs_vec_push(&args, strdup(url));

    hs_exec_result res = hs_exec_args(&args);
    hs_strvec_free(&args);

    if (!hs_exec_success(&res)) {
        hs_error("failed to fetch %s: %s", url, res.stderr_output ? res.stderr_output : "");
        hs_exec_result_free(&res);
        return NULL;
    }

    char *output = res.stdout_output;
    res.stdout_output = NULL;
    hs_exec_result_free(&res);
    return output;
}

static char *extract_json_string(const char *json, size_t json_len, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos || (size_t)(pos - json) >= json_len) return strdup("");

    pos = strchr(pos, ':');
    if (!pos || (size_t)(pos - json) >= json_len) return strdup("");

    pos = strchr(pos, '"');
    if (!pos || (size_t)(pos - json) >= json_len) return strdup("");

    const char *start = pos + 1;
    const char *end = strchr(start, '"');
    if (!end || (size_t)(end - json) >= json_len) return strdup("");

    size_t len = (size_t)(end - start);
    char *result = malloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

static int extract_json_bool(const char *json, size_t json_len, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos || (size_t)(pos - json) >= json_len) return 0;

    pos = strchr(pos, ':');
    if (!pos || (size_t)(pos - json) >= json_len) return 0;

    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') {
        if ((size_t)(pos - json) >= json_len) return 0;
        pos++;
    }

    return strncmp(pos, "true", 4) == 0;
}

static int release_cmp_desc(const void *a, const void *b) {
    const hs_release *ra = (const hs_release *)a;
    const hs_release *rb = (const hs_release *)b;
    return hs_version_cmp(rb->version, ra->version);
}

static hs_releasevec parse_releases_json(const char *json) {
    hs_releasevec releases;
    hs_vec_init(&releases);

    if (!json) return releases;

    size_t json_len = strlen(json);
    const char *pos = json;

    while ((pos = strstr(pos, "\"tag_name\"")) != NULL) {
        const char *block_start = pos;
        while (block_start > json && *block_start != '{') block_start--;
        if (*block_start != '{') { pos++; continue; }

        const char *block_end = strchr(pos, '}');
        if (!block_end) { pos++; continue; }

        size_t block_len = (size_t)(block_end - block_start + 1);

        char *block = malloc(block_len + 1);
        memcpy(block, block_start, block_len);
        block[block_len] = '\0';

        hs_release release;
        memset(&release, 0, sizeof(release));

        release.tag_name = extract_json_string(block, block_len, "tag_name");
        release.name = extract_json_string(block, block_len, "name");
        release.body = strdup("");
        release.published_at = extract_json_string(block, block_len, "published_at");
        release.prerelease = extract_json_bool(block, block_len, "prerelease");
        release.download_url = strdup("");

        free(block);

        hs_version v;
        if (hs_version_parse(release.tag_name, &v)) {
            release.version = v;

            const char *assets_pos = strstr(block_start, "\"assets\"");
            if (assets_pos && (size_t)(assets_pos - json) < (size_t)(block_end - json) + 500 &&
                (size_t)(assets_pos - json) < json_len) {
                const char *url_pos = strstr(assets_pos, "browser_download_url");
                if (url_pos && (size_t)(url_pos - json) < json_len) {
                    const char *url_start = strchr(url_pos + 20, '"');
                    if (url_start && (size_t)(url_start - json) < json_len) {
                        url_start++;
                        const char *url_end = strchr(url_start, '"');
                        if (url_end && (size_t)(url_end - json) < json_len) {
                            size_t url_len = (size_t)(url_end - url_start);
                            char *url = malloc(url_len + 1);
                            memcpy(url, url_start, url_len);
                            url[url_len] = '\0';

                            if (strstr(url, "linux") != NULL) {
                                free(release.download_url);
                                release.download_url = url;
                            } else {
                                free(url);
                            }
                        }
                    }
                }
            }

            hs_vec_push(&releases, release);
        } else {
            hs_release_free(&release);
        }

        pos = block_end;
    }

    if (releases.len > 1) {
        qsort(releases.data, releases.len, sizeof(hs_release), release_cmp_desc);
    }

    return releases;
}

hs_releasevec hs_fetch_releases(void) {
    char *json = fetch_github_api(HS_GITHUB_API_URL);
    if (!json) {
        hs_releasevec empty;
        hs_vec_init(&empty);
        return empty;
    }

    hs_releasevec releases = parse_releases_json(json);
    free(json);
    return releases;
}

int hs_get_latest_release(hs_release *out) {
    hs_releasevec releases = hs_fetch_releases();

    for (size_t i = 0; i < releases.len; i++) {
        if (!releases.data[i].prerelease) {
            *out = releases.data[i];
            releases.data[i].tag_name = NULL;
            releases.data[i].name = NULL;
            releases.data[i].body = NULL;
            releases.data[i].published_at = NULL;
            releases.data[i].download_url = NULL;
            hs_releasevec_free(&releases);
            return 1;
        }
    }

    hs_releasevec_free(&releases);
    return 0;
}

int hs_get_latest_dev_release(hs_release *out) {
    hs_releasevec releases = hs_fetch_releases();

    for (size_t i = 0; i < releases.len; i++) {
        if (releases.data[i].prerelease) {
            *out = releases.data[i];
            releases.data[i].tag_name = NULL;
            releases.data[i].name = NULL;
            releases.data[i].body = NULL;
            releases.data[i].published_at = NULL;
            releases.data[i].download_url = NULL;
            hs_releasevec_free(&releases);
            return 1;
        }
    }

    hs_releasevec_free(&releases);
    return 0;
}

int hs_get_release_by_version(const char *version, hs_release *out) {
    hs_version target;
    if (!hs_version_parse(version, &target)) {
        hs_error("invalid version format: %s", version);
        return 0;
    }

    hs_releasevec releases = hs_fetch_releases();

    for (size_t i = 0; i < releases.len; i++) {
        if (hs_version_eq(releases.data[i].version, target)) {
            *out = releases.data[i];
            releases.data[i].tag_name = NULL;
            releases.data[i].name = NULL;
            releases.data[i].body = NULL;
            releases.data[i].published_at = NULL;
            releases.data[i].download_url = NULL;
            hs_releasevec_free(&releases);
            return 1;
        }
    }

    hs_releasevec_free(&releases);
    return 0;
}

int hs_has_update(void) {
    hs_release latest;
    if (!hs_get_latest_release(&latest)) return 0;

    int result = hs_version_cmp(latest.version, hs_current_version()) > 0;
    hs_release_free(&latest);
    return result;
}

static int download_file(const char *url, const char *dest) {
    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("curl"));
    hs_vec_push(&args, strdup("-s"));
    hs_vec_push(&args, strdup("-L"));
    hs_vec_push(&args, strdup("-o"));
    hs_vec_push(&args, strdup(dest));
    hs_vec_push(&args, strdup(url));

    hs_exec_result res = hs_exec_args(&args);
    hs_strvec_free(&args);

    int ok = hs_exec_success(&res);
    hs_exec_result_free(&res);
    return ok;
}

static int replace_binary(const char *new_binary, const char *binary_path) {
    if (chmod(new_binary, 0755) != 0) {
        hs_error("failed to set executable permission on %s", new_binary);
        return 0;
    }

    char *backup_path = malloc(strlen(binary_path) + 8);
    sprintf(backup_path, "%s.backup", binary_path);

    hs_strvec cp_args;
    hs_vec_init(&cp_args);
    hs_vec_push(&cp_args, strdup("cp"));
    hs_vec_push(&cp_args, strdup("-f"));
    hs_vec_push(&cp_args, strdup(binary_path));
    hs_vec_push(&cp_args, strdup(backup_path));
    hs_exec_result cp_res = hs_exec_args(&cp_args);
    hs_strvec_free(&cp_args);
    if (!hs_exec_success(&cp_res)) {
        hs_warn("could not create backup: %s", cp_res.stderr_output ? cp_res.stderr_output : "");
    }
    hs_exec_result_free(&cp_res);
    free(backup_path);

    if (rename(new_binary, binary_path) == 0) {
        return 1;
    }

    hs_strvec mv_args;
    hs_vec_init(&mv_args);
    hs_vec_push(&mv_args, strdup("sudo"));
    hs_vec_push(&mv_args, strdup("mv"));
    hs_vec_push(&mv_args, strdup(new_binary));
    hs_vec_push(&mv_args, strdup(binary_path));
    hs_exec_result mv_res = hs_exec_args(&mv_args);
    hs_strvec_free(&mv_args);

    if (!hs_exec_success(&mv_res)) {
        hs_error("failed to replace binary: %s", mv_res.stderr_output ? mv_res.stderr_output : "");
        hs_exec_result_free(&mv_res);
        return 0;
    }
    hs_exec_result_free(&mv_res);
    return 1;
}

int hs_upgrade(const hs_release *release) {
    hs_install_method method = hs_detect_install_method();

    if (method == HS_INSTALL_PACKAGE_MANAGER) {
        printf("hyprsync is installed via package manager.\n");
        printf("please upgrade using your package manager:\n\n");

        if (hs_file_exists("/usr/bin/pacman")) {
            printf("  yay -S hyprsync\n");
            printf("  # or\n");
            printf("  paru -S hyprsync\n");
        } else if (hs_file_exists("/usr/bin/apt")) {
            printf("  sudo apt update && sudo apt upgrade hyprsync\n");
        } else if (hs_file_exists("/usr/bin/dnf")) {
            printf("  sudo dnf upgrade hyprsync\n");
        } else {
            printf("  use your system's package manager to upgrade\n");
        }

        char *ver = hs_version_to_string(release->version);
        printf("\nnew version available: %s\n", ver);
        free(ver);
        return 0;
    }

    if (!release->download_url || release->download_url[0] == '\0') {
        hs_error("no download url available for version %s", release->tag_name);
        return 0;
    }

    char *ver = hs_version_to_string(release->version);
    printf("downloading hyprsync %s...\n", ver);

    const char *temp_path = "/tmp/hyprsync-update";

    if (!download_file(release->download_url, temp_path)) {
        hs_error("failed to download update");
        free(ver);
        return 0;
    }

    printf("installing...\n");

    char *binary_path = hs_get_binary_path();
    if (!replace_binary(temp_path, binary_path)) {
        hs_error("failed to install update");
        free(binary_path);
        free(ver);
        return 0;
    }
    free(binary_path);

    write_install_marker(HS_INSTALL_SCRIPT);

    printf("successfully upgraded to %s\n", ver);
    free(ver);
    return 1;
}

int hs_upgrade_to_latest(void) {
    printf("checking for updates...\n\n");

    hs_releasevec releases = hs_fetch_releases();
    if (releases.len == 0) {
        fprintf(stderr, "error: could not fetch releases from GitHub\n");
        fprintf(stderr, "check your internet connection and try again\n");
        hs_releasevec_free(&releases);
        return 0;
    }

    hs_release *latest = NULL;
    for (size_t i = 0; i < releases.len; i++) {
        if (!releases.data[i].prerelease) {
            latest = &releases.data[i];
            break;
        }
    }

    if (!latest) {
        hs_version current = hs_current_version();
        char *cur_str = hs_version_to_string(current);
        printf("no stable releases available yet\n");
        printf("current version: %s\n\n", cur_str);
        free(cur_str);

        for (size_t i = 0; i < releases.len; i++) {
            if (releases.data[i].prerelease) {
                char *dev_str = hs_version_to_string(releases.data[i].version);
                printf("development version available: %s\n", dev_str);
                printf("install with: curl -fsSL https://raw.githubusercontent.com/r2unit/hyprsync/devel/install.sh | bash -s -- --dev\n");
                free(dev_str);
                break;
            }
        }
        hs_releasevec_free(&releases);
        return 1;
    }

    hs_version current = hs_current_version();

    if (hs_version_eq(latest->version, current)) {
        char *cur_str = hs_version_to_string(current);
        printf("you are on the latest version (%s) :)\n", cur_str);
        free(cur_str);
        hs_releasevec_free(&releases);
        return 1;
    }

    if (hs_version_cmp(latest->version, current) < 0) {
        char *cur_str = hs_version_to_string(current);
        char *lat_str = hs_version_to_string(latest->version);
        printf("you are running a newer version than the latest release :)\n");
        printf("  current: %s\n", cur_str);
        printf("  latest:  %s\n", lat_str);
        free(cur_str);
        free(lat_str);
        hs_releasevec_free(&releases);
        return 1;
    }

    char *cur_str = hs_version_to_string(current);
    char *lat_str = hs_version_to_string(latest->version);
    printf("new version available!\n");
    printf("  current: %s\n", cur_str);
    printf("  latest:  %s\n\n", lat_str);
    free(cur_str);
    free(lat_str);

    int result = hs_upgrade(latest);
    hs_releasevec_free(&releases);
    return result;
}

int hs_upgrade_to_latest_dev(void) {
    printf("checking for development builds...\n\n");

    hs_releasevec releases = hs_fetch_releases();
    if (releases.len == 0) {
        fprintf(stderr, "error: could not fetch releases from GitHub\n");
        fprintf(stderr, "check your internet connection and try again\n");
        hs_releasevec_free(&releases);
        return 0;
    }

    hs_release *latest_dev = NULL;
    for (size_t i = 0; i < releases.len; i++) {
        if (releases.data[i].prerelease) {
            latest_dev = &releases.data[i];
            break;
        }
    }

    if (!latest_dev) {
        printf("no development builds available\n");
        hs_releasevec_free(&releases);
        return 1;
    }

    hs_version current = hs_current_version();
    char *cur_str = hs_version_to_string(current);
    printf("latest dev build: %s\n", latest_dev->tag_name);
    printf("current version:  %s\n\n", cur_str);
    free(cur_str);

    int result = hs_upgrade(latest_dev);
    hs_releasevec_free(&releases);
    return result;
}

int hs_upgrade_to_version(const char *version) {
    printf("looking for version %s...\n\n", version);

    hs_release release;
    if (!hs_get_release_by_version(version, &release)) {
        fprintf(stderr, "error: version %s not found\n", version);
        fprintf(stderr, "run 'hyprsync upgrade list' to see available versions\n");
        return 0;
    }

    hs_version current = hs_current_version();
    if (hs_version_eq(release.version, current)) {
        printf("you are already on version %s :)\n", version);
        hs_release_free(&release);
        return 1;
    }

    int result = hs_upgrade(&release);
    hs_release_free(&release);
    return result;
}

void hs_list_available_versions(void) {
    printf("fetching available versions...\n\n");

    hs_releasevec releases = hs_fetch_releases();

    if (releases.len == 0) {
        fprintf(stderr, "error: could not fetch releases from GitHub\n");
        fprintf(stderr, "check your internet connection and try again\n");
        hs_releasevec_free(&releases);
        return;
    }

    hs_version current = hs_current_version();

    printf("available versions:\n\n");
    for (size_t i = 0; i < releases.len; i++) {
        hs_release *r = &releases.data[i];
        char *ver = hs_version_to_string(r->version);
        printf("  %s", ver);
        free(ver);

        if (hs_version_eq(r->version, current)) {
            printf(" <- installed");
        } else if (hs_version_cmp(r->version, current) > 0) {
            printf(" (newer)");
        }

        if (r->prerelease) {
            printf(" [dev]");
        }

        if (r->published_at && r->published_at[0] != '\0' && strlen(r->published_at) >= 10) {
            printf(" - %.10s", r->published_at);
        }

        printf("\n");
    }
    printf("\n");

    hs_releasevec_free(&releases);
}

void hs_release_free(hs_release *r) {
    if (!r) return;
    free(r->tag_name);
    free(r->name);
    free(r->body);
    free(r->published_at);
    free(r->download_url);
    r->tag_name = NULL;
    r->name = NULL;
    r->body = NULL;
    r->published_at = NULL;
    r->download_url = NULL;
}

void hs_releasevec_free(hs_releasevec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) {
        hs_release_free(&v->data[i]);
    }
    hs_vec_free(v);
}

const char *hs_install_method_to_string(hs_install_method method) {
    switch (method) {
        case HS_INSTALL_SCRIPT: return "script";
        case HS_INSTALL_PACKAGE_MANAGER: return "package";
        default: return "unknown";
    }
}
