#include "git.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace hyprsync {

GitManager::GitManager(const GitConfig& config, const std::string& hostname)
    : config_(config)
    , hostname_(hostname)
    , repo_path_(expand_path(config.repo))
    , home_dir_(get_home_dir()) {
}

bool GitManager::init_repo() {
    if (is_initialized()) {
        spdlog::debug("git repo already initialized at {}", repo_path_.string());
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(repo_path_, ec);
    if (ec) {
        spdlog::error("failed to create repo directory: {}", ec.message());
        return false;
    }

    std::filesystem::permissions(repo_path_,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace, ec);

    auto result = git_exec({"init"});
    if (!result.success()) {
        spdlog::error("git init failed: {}", result.stderr_output);
        return false;
    }

    result = git_exec({"config", "user.name", "hyprsync"});
    if (!result.success()) {
        spdlog::warn("failed to set git user.name");
    }

    result = git_exec({"config", "user.email", "hyprsync@localhost"});
    if (!result.success()) {
        spdlog::warn("failed to set git user.email");
    }

    spdlog::info("initialized git repo at {}", repo_path_.string());
    return true;
}

bool GitManager::is_initialized() const {
    return dir_exists(repo_path_ / ".git");
}

void GitManager::snapshot(const std::vector<SyncGroup>& groups) {
    update_gitignore(groups);

    for (const auto& group : groups) {
        for (const auto& path : group.paths) {
            auto expanded = expand_path(path);
            if (file_exists(expanded) || dir_exists(expanded)) {
                copy_to_repo(expanded);
            } else {
                spdlog::debug("skipping non-existent path: {}", expanded.string());
            }
        }
    }

    git_exec({"add", "-A"});
}

void GitManager::restore(const std::vector<SyncGroup>& groups) {
    for (const auto& group : groups) {
        for (const auto& path : group.paths) {
            auto repo_relative = to_repo_path(expand_path(path));
            auto repo_file = repo_path_ / repo_relative;

            if (file_exists(repo_file) || dir_exists(repo_file)) {
                copy_from_repo(repo_file);
            }
        }
    }
}

bool GitManager::commit(const std::string& message) {
    if (!has_changes()) {
        spdlog::debug("no changes to commit");
        return true;
    }

    auto result = git_exec({"commit", "-m", message});
    if (!result.success()) {
        spdlog::error("commit failed: {}", result.stderr_output);
        return false;
    }

    spdlog::info("committed: {}", message);
    return true;
}

bool GitManager::has_changes() const {
    auto result = git_exec({"status", "--porcelain"});
    return result.success() && !trim(result.stdout_output).empty();
}

std::string GitManager::diff() const {
    auto result = git_exec({"diff"});
    return result.stdout_output;
}

std::string GitManager::diff_staged() const {
    auto result = git_exec({"diff", "--cached"});
    return result.stdout_output;
}

std::string GitManager::diff_remote(const std::string& device) const {
    std::string branch = "remote/" + device;
    auto result = git_exec({"diff", branch});
    return result.stdout_output;
}

std::vector<std::string> GitManager::log(int count) const {
    std::vector<std::string> entries;
    auto result = git_exec({"log", "--oneline", "-n", std::to_string(count)});

    if (result.success()) {
        std::istringstream stream(result.stdout_output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                entries.push_back(line);
            }
        }
    }

    return entries;
}

std::vector<std::string> GitManager::changed_files() const {
    std::vector<std::string> files;
    auto result = git_exec({"status", "--porcelain"});

    if (result.success()) {
        std::istringstream stream(result.stdout_output);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.length() > 3) {
                files.push_back(line.substr(3));
            }
        }
    }

    return files;
}

std::vector<ConflictFile> GitManager::get_conflicts() const {
    std::vector<ConflictFile> conflicts;
    auto result = git_exec({"diff", "--name-only", "--diff-filter=U"});

    if (result.success()) {
        std::istringstream stream(result.stdout_output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                ConflictFile cf;
                cf.path = line;
                conflicts.push_back(cf);
            }
        }
    }

    return conflicts;
}

bool GitManager::resolve_conflict(const std::filesystem::path& file,
                                   ConflictStrategy strategy) {
    ExecResult result;

    switch (strategy) {
        case ConflictStrategy::NewestWins: {
            auto repo_file = repo_path_ / file;
            auto original = to_original_path(file);

            if (file_exists(repo_file) && file_exists(original)) {
                auto repo_time = std::filesystem::last_write_time(repo_file);
                auto orig_time = std::filesystem::last_write_time(original);

                if (orig_time > repo_time) {
                    result = git_exec({"checkout", "--ours", file.string()});
                } else {
                    result = git_exec({"checkout", "--theirs", file.string()});
                }
            } else {
                result = git_exec({"checkout", "--theirs", file.string()});
            }
            break;
        }

        case ConflictStrategy::KeepBoth: {
            auto repo_file = repo_path_ / file;
            auto ours_file = repo_file.string() + "." + hostname_;
            auto theirs_file = repo_file.string() + ".remote";

            git_exec({"show", ":2:" + file.string()});
            git_exec({"show", ":3:" + file.string()});
            result = git_exec({"checkout", "--theirs", file.string()});
            break;
        }

        case ConflictStrategy::Manual:
        default:
            spdlog::info("manual resolution required for: {}", file.string());
            return false;
    }

    if (result.success()) {
        git_exec({"add", file.string()});
        return true;
    }

    return false;
}

bool GitManager::has_conflicts() const {
    return !get_conflicts().empty();
}

void GitManager::create_device_branch(const std::string& device) {
    std::string branch = "remote/" + device;
    git_exec({"branch", branch});
}

void GitManager::update_device_branch(const std::string& device) {
    std::string branch = "remote/" + device;
    git_exec({"branch", "-f", branch, "HEAD"});
}

std::filesystem::path GitManager::to_repo_path(const std::filesystem::path& original) const {
    auto expanded = expand_path(original);
    std::string path_str = expanded.string();
    std::string home_str = home_dir_.string();

    if (path_str.find(home_str) == 0) {
        std::string relative = path_str.substr(home_str.length());
        if (!relative.empty() && relative[0] == '/') {
            relative = relative.substr(1);
        }
        return std::filesystem::path(relative);
    }

    return expanded;
}

std::filesystem::path GitManager::to_original_path(const std::filesystem::path& repo_relative) const {
    return home_dir_ / repo_relative;
}

ExecResult GitManager::git_exec(const std::vector<std::string>& args) const {
    std::vector<std::string> full_args = {"git", "-C", repo_path_.string()};
    full_args.insert(full_args.end(), args.begin(), args.end());
    return exec(full_args);
}

void GitManager::update_gitignore(const std::vector<SyncGroup>& groups) {
    std::set<std::string> excludes;

    excludes.insert(".ssh/");
    excludes.insert(".gnupg/");
    excludes.insert("*secret*");
    excludes.insert("*token*");
    excludes.insert("*.key");
    excludes.insert("*.pem");

    for (const auto& group : groups) {
        for (const auto& exc : group.exclude) {
            excludes.insert(exc);
        }
    }

    auto gitignore_path = repo_path_ / ".gitignore";
    std::ofstream file(gitignore_path);
    if (file.is_open()) {
        file << "# Generated by hyprsync\n";
        for (const auto& exc : excludes) {
            file << exc << "\n";
        }
        file.close();
    }
}

void GitManager::copy_to_repo(const std::filesystem::path& source) {
    auto relative = to_repo_path(source);
    auto dest = repo_path_ / relative;

    std::error_code ec;

    if (std::filesystem::is_directory(source)) {
        std::filesystem::create_directories(dest, ec);
        if (ec) {
            spdlog::error("failed to create directory {}: {}", dest.string(), ec.message());
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
            auto entry_relative = std::filesystem::relative(entry.path(), source);
            auto entry_dest = dest / entry_relative;

            if (entry.is_directory()) {
                std::filesystem::create_directories(entry_dest, ec);
            } else if (entry.is_regular_file()) {
                std::filesystem::create_directories(entry_dest.parent_path(), ec);
                std::filesystem::copy_file(entry.path(), entry_dest,
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    spdlog::error("failed to copy {}: {}", entry.path().string(), ec.message());
                }
            }
        }
    } else if (std::filesystem::is_regular_file(source)) {
        std::filesystem::create_directories(dest.parent_path(), ec);
        std::filesystem::copy_file(source, dest,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            spdlog::error("failed to copy {}: {}", source.string(), ec.message());
        }
    }
}

void GitManager::copy_from_repo(const std::filesystem::path& repo_file) {
    auto relative = std::filesystem::relative(repo_file, repo_path_);
    auto dest = to_original_path(relative);

    std::error_code ec;

    if (std::filesystem::is_directory(repo_file)) {
        std::filesystem::create_directories(dest, ec);
        if (ec) {
            spdlog::error("failed to create directory {}: {}", dest.string(), ec.message());
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(repo_file)) {
            auto entry_relative = std::filesystem::relative(entry.path(), repo_file);
            auto entry_dest = dest / entry_relative;

            if (entry.is_directory()) {
                std::filesystem::create_directories(entry_dest, ec);
            } else if (entry.is_regular_file()) {
                std::filesystem::create_directories(entry_dest.parent_path(), ec);
                std::filesystem::copy_file(entry.path(), entry_dest,
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    spdlog::error("failed to copy {}: {}", entry.path().string(), ec.message());
                }
            }
        }
    } else if (std::filesystem::is_regular_file(repo_file)) {
        std::filesystem::create_directories(dest.parent_path(), ec);
        std::filesystem::copy_file(repo_file, dest,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            spdlog::error("failed to copy {}: {}", repo_file.string(), ec.message());
        }
    }
}

}
