#pragma once

#include "config.hpp"
#include "util.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace hyprsync {

struct ConflictFile {
    std::filesystem::path path;
    std::string ours;
    std::string theirs;
};

class GitManager {
public:
    GitManager(const GitConfig& config, const std::string& hostname);

    bool init_repo();
    bool is_initialized() const;

    void snapshot(const std::vector<SyncGroup>& groups);
    void snapshot_changed(const std::vector<std::filesystem::path>& changed_paths,
                          const std::vector<SyncGroup>& groups);
    void restore(const std::vector<SyncGroup>& groups);

    bool commit(const std::string& message);
    bool has_changes() const;

    std::string diff() const;
    std::string diff_staged() const;
    std::string diff_remote(const std::string& device) const;

    std::vector<std::string> log(int count = 20) const;
    std::vector<std::string> changed_files() const;

    std::vector<ConflictFile> get_conflicts() const;
    bool resolve_conflict(const std::filesystem::path& file, ConflictStrategy strategy);
    bool has_conflicts() const;

    void create_device_branch(const std::string& device);
    void update_device_branch(const std::string& device);

    std::filesystem::path repo_path() const { return repo_path_; }

    std::filesystem::path to_repo_path(const std::filesystem::path& original) const;
    std::filesystem::path to_original_path(const std::filesystem::path& repo_relative) const;

private:
    GitConfig config_;
    std::string hostname_;
    std::filesystem::path repo_path_;
    std::filesystem::path home_dir_;

    ExecResult git_exec(const std::vector<std::string>& args) const;

    void update_gitignore(const std::vector<SyncGroup>& groups);
    void copy_to_repo(const std::filesystem::path& source);
    void copy_from_repo(const std::filesystem::path& repo_file);
};

}
