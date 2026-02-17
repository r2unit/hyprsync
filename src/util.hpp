#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace hyprsync {

struct ExecResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;

    bool success() const { return exit_code == 0; }
};

std::filesystem::path expand_path(const std::filesystem::path& path);

std::optional<std::string> get_env(const std::string& name);

std::filesystem::path get_home_dir();

std::string get_hostname();

ExecResult exec(const std::string& command);

ExecResult exec(const std::vector<std::string>& args);

ExecResult exec(const std::vector<std::string>& args,
                const std::filesystem::path& working_dir);

std::string trim(const std::string& str);

std::vector<std::string> split(const std::string& str, char delimiter);

bool file_exists(const std::filesystem::path& path);

bool dir_exists(const std::filesystem::path& path);

}
