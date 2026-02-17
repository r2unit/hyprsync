#include "util.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>

namespace hyprsync {

std::filesystem::path expand_path(const std::filesystem::path& path) {
    std::string path_str = path.string();

    if (path_str.empty()) {
        return path;
    }

    if (path_str[0] == '~') {
        std::filesystem::path home = get_home_dir();
        if (path_str.size() == 1) {
            return home;
        }
        if (path_str[1] == '/') {
            return home / path_str.substr(2);
        }
    }

    if (path_str.find("$HOME") != std::string::npos) {
        std::string home_str = get_home_dir().string();
        size_t pos = 0;
        while ((pos = path_str.find("$HOME", pos)) != std::string::npos) {
            path_str.replace(pos, 5, home_str);
            pos += home_str.length();
        }
        return std::filesystem::path(path_str);
    }

    return path;
}

std::optional<std::string> get_env(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

std::filesystem::path get_home_dir() {
    auto home = get_env("HOME");
    if (home.has_value()) {
        return std::filesystem::path(home.value());
    }

    struct passwd* pw = getpwuid(getuid());
    if (pw != nullptr && pw->pw_dir != nullptr) {
        return std::filesystem::path(pw->pw_dir);
    }

    throw std::runtime_error("could not determine home directory");
}

std::string get_hostname() {
    std::array<char, 256> buffer{};
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        return std::string(buffer.data());
    }

    auto hostname = get_env("HOSTNAME");
    if (hostname.has_value()) {
        return hostname.value();
    }

    return "unknown";
}

ExecResult exec(const std::string& command) {
    ExecResult result{};

    std::array<int, 2> stdout_pipe{};
    std::array<int, 2> stderr_pipe{};

    if (pipe(stdout_pipe.data()) != 0 || pipe(stderr_pipe.data()) != 0) {
        result.exit_code = -1;
        result.stderr_output = "failed to create pipes";
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.exit_code = -1;
        result.stderr_output = "fork failed";
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

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    std::array<char, 4096> buffer{};
    ssize_t bytes_read;

    while ((bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
        result.stdout_output.append(buffer.data(), bytes_read);
    }
    close(stdout_pipe[0]);

    while ((bytes_read = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
        result.stderr_output.append(buffer.data(), bytes_read);
    }
    close(stderr_pipe[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = -1;
    }

    return result;
}

ExecResult exec(const std::vector<std::string>& args) {
    if (args.empty()) {
        return ExecResult{-1, "", "empty command"};
    }

    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd << ' ';

        bool needs_quoting = args[i].find_first_of(" \t\n\"'\\$`") != std::string::npos;
        if (needs_quoting) {
            cmd << '"';
            for (char c : args[i]) {
                if (c == '"' || c == '\\' || c == '$' || c == '`') {
                    cmd << '\\';
                }
                cmd << c;
            }
            cmd << '"';
        } else {
            cmd << args[i];
        }
    }

    return exec(cmd.str());
}

ExecResult exec(const std::vector<std::string>& args,
                const std::filesystem::path& working_dir) {
    std::string current_dir = std::filesystem::current_path().string();
    std::filesystem::current_path(working_dir);

    ExecResult result = exec(args);

    std::filesystem::current_path(current_dir);

    return result;
}

std::string trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool file_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) &&
           std::filesystem::is_regular_file(path, ec);
}

bool dir_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) &&
           std::filesystem::is_directory(path, ec);
}

}
