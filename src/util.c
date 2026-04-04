#define _POSIX_C_SOURCE 200809L

#include "util.h"
#include "log.h"

#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

char *hs_strdup_safe(const char *s) {
    if (!s) return NULL;
    return strdup(s);
}

char *hs_join_path(const char *base, const char *rel) {
    if (!base || !rel) return hs_strdup_safe(base ? base : rel);
    size_t blen = strlen(base);
    size_t rlen = strlen(rel);
    int need_sep = (blen > 0 && base[blen - 1] != '/');
    size_t total = blen + need_sep + rlen + 1;
    char *out = malloc(total);
    if (!out) return NULL;
    memcpy(out, base, blen);
    if (need_sep) out[blen] = '/';
    memcpy(out + blen + need_sep, rel, rlen);
    out[total - 1] = '\0';
    return out;
}

void hs_exec_result_free(hs_exec_result *r) {
    if (!r) return;
    free(r->stdout_output);
    free(r->stderr_output);
    r->stdout_output = NULL;
    r->stderr_output = NULL;
}

char *hs_get_env(const char *name) {
    const char *value = getenv(name);
    if (!value) return NULL;
    return strdup(value);
}

char *hs_get_home_dir(void) {
    char *home = hs_get_env("HOME");
    if (home) return home;

    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return strdup(pw->pw_dir);
    }

    hs_error("could not determine home directory");
    return NULL;
}

char *hs_expand_path(const char *path) {
    if (!path || !path[0]) {
        return hs_strdup_safe(path);
    }

    if (path[0] == '~') {
        char *home = hs_get_home_dir();
        if (!home) return strdup(path);
        if (path[1] == '\0') {
            return home;
        }
        if (path[1] == '/') {
            char *result = hs_join_path(home, path + 2);
            free(home);
            return result;
        }
        free(home);
        return strdup(path);
    }

    if (strstr(path, "$HOME")) {
        char *home = hs_get_home_dir();
        if (!home) return strdup(path);
        size_t home_len = strlen(home);
        size_t path_len = strlen(path);

        size_t count = 0;
        const char *p = path;
        while ((p = strstr(p, "$HOME")) != NULL) {
            count++;
            p += 5;
        }

        size_t result_len = path_len + count * (home_len - 5) + 1;
        char *result = malloc(result_len);
        if (!result) {
            free(home);
            return strdup(path);
        }

        char *dst = result;
        const char *src = path;
        while (*src) {
            if (strncmp(src, "$HOME", 5) == 0) {
                memcpy(dst, home, home_len);
                dst += home_len;
                src += 5;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        free(home);
        return result;
    }

    return strdup(path);
}

char *hs_get_hostname(void) {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        buffer[sizeof(buffer) - 1] = '\0';
        return strdup(buffer);
    }

    char *hostname = hs_get_env("HOSTNAME");
    if (hostname) return hostname;

    return strdup("unknown");
}

static void append_buf(char **buf, size_t *len, size_t *cap, const char *data, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t newcap = (*cap) ? (*cap) * 2 : 4096;
        while (newcap < *len + n + 1) newcap *= 2;
        *buf = realloc(*buf, newcap);
        *cap = newcap;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = '\0';
}

hs_exec_result hs_exec(const char *command) {
    hs_exec_result result;
    result.exit_code = 0;
    result.stdout_output = strdup("");
    result.stderr_output = strdup("");

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        free(result.stdout_output);
        free(result.stderr_output);
        result.exit_code = -1;
        result.stdout_output = strdup("");
        result.stderr_output = strdup("failed to create pipes");
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        free(result.stdout_output);
        free(result.stderr_output);
        result.exit_code = -1;
        result.stdout_output = strdup("");
        result.stderr_output = strdup("fork failed");
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    char readbuf[4096];
    size_t out_len = 0, out_cap = 0;
    size_t err_len = 0, err_cap = 0;
    char *out_buf = NULL;
    char *err_buf = NULL;

    struct pollfd fds[2];
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    int open_fds = 2;
    while (open_fds > 0) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int f = 0; f < 2; f++) {
            if (fds[f].fd < 0) continue;
            if (fds[f].revents & (POLLIN | POLLHUP)) {
                ssize_t bytes_read = read(fds[f].fd, readbuf, sizeof(readbuf));
                if (bytes_read > 0) {
                    if (f == 0) {
                        append_buf(&out_buf, &out_len, &out_cap, readbuf, (size_t)bytes_read);
                    } else {
                        append_buf(&err_buf, &err_len, &err_cap, readbuf, (size_t)bytes_read);
                    }
                } else {
                    close(fds[f].fd);
                    fds[f].fd = -1;
                    open_fds--;
                }
            }
        }
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = -1;
    }

    free(result.stdout_output);
    free(result.stderr_output);
    result.stdout_output = out_buf ? out_buf : strdup("");
    result.stderr_output = err_buf ? err_buf : strdup("");

    return result;
}

static int needs_shell_quoting(const char *s) {
    for (; *s; s++) {
        char c = *s;
        if (c == ' ' || c == '\t' || c == '\n' || c == '"' ||
            c == '\'' || c == '\\' || c == '$' || c == '`')
            return 1;
    }
    return 0;
}

static void append_quoted_arg(char **buf, size_t *len, size_t *cap, const char *arg) {
    if (needs_shell_quoting(arg)) {
        append_buf(buf, len, cap, "\"", 1);
        for (const char *p = arg; *p; p++) {
            if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
                append_buf(buf, len, cap, "\\", 1);
            }
            append_buf(buf, len, cap, p, 1);
        }
        append_buf(buf, len, cap, "\"", 1);
    } else {
        append_buf(buf, len, cap, arg, strlen(arg));
    }
}

hs_exec_result hs_exec_args(const hs_strvec *args) {
    if (!args || args->len == 0) {
        hs_exec_result result;
        result.exit_code = -1;
        result.stdout_output = strdup("");
        result.stderr_output = strdup("empty command");
        return result;
    }

    char *cmd = NULL;
    size_t cmd_len = 0, cmd_cap = 0;

    for (size_t i = 0; i < args->len; i++) {
        if (i > 0) append_buf(&cmd, &cmd_len, &cmd_cap, " ", 1);
        append_quoted_arg(&cmd, &cmd_len, &cmd_cap, args->data[i]);
    }

    hs_exec_result result = hs_exec(cmd);
    free(cmd);
    return result;
}

hs_exec_result hs_exec_args_dir(const hs_strvec *args, const char *workdir) {
    if (!args || args->len == 0) {
        hs_exec_result result;
        result.exit_code = -1;
        result.stdout_output = strdup("");
        result.stderr_output = strdup("empty command");
        return result;
    }

    char *cmd = NULL;
    size_t cmd_len = 0, cmd_cap = 0;

    append_buf(&cmd, &cmd_len, &cmd_cap, "cd ", 3);
    append_quoted_arg(&cmd, &cmd_len, &cmd_cap, workdir);
    append_buf(&cmd, &cmd_len, &cmd_cap, " && ", 4);

    for (size_t i = 0; i < args->len; i++) {
        if (i > 0) append_buf(&cmd, &cmd_len, &cmd_cap, " ", 1);
        append_quoted_arg(&cmd, &cmd_len, &cmd_cap, args->data[i]);
    }

    hs_exec_result result = hs_exec(cmd);
    free(cmd);
    return result;
}

char *hs_trim(const char *str) {
    if (!str) return strdup("");
    const char *whitespace = " \t\n\r\f\v";
    const char *start = str;
    while (*start && strchr(whitespace, *start)) start++;
    if (!*start) return strdup("");
    const char *end = str + strlen(str) - 1;
    while (end > start && strchr(whitespace, *end)) end--;
    size_t len = (size_t)(end - start + 1);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

hs_strvec hs_split(const char *str, char delimiter) {
    hs_strvec tokens;
    hs_vec_init(&tokens);

    if (!str) return tokens;

    const char *start = str;
    while (1) {
        const char *end = strchr(start, delimiter);
        if (!end) {
            hs_vec_push(&tokens, strdup(start));
            break;
        }
        size_t len = (size_t)(end - start);
        char *token = malloc(len + 1);
        memcpy(token, start, len);
        token[len] = '\0';
        hs_vec_push(&tokens, token);
        start = end + 1;
    }

    return tokens;
}

int hs_file_exists(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int hs_dir_exists(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}
