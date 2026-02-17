#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hyprsync {

enum class SyncMode {
    Push,
    Pull,
    Bidirectional
};

enum class ConflictStrategy {
    NewestWins,
    Manual,
    KeepBoth
};

struct Device {
    std::string name;
    std::string host;
    std::string user;
    int port = 22;
    std::string key;
};

struct SyncGroup {
    std::string name;
    std::vector<std::filesystem::path> paths;
    std::vector<std::string> exclude;
    std::vector<std::string> devices;
    std::string remote_path;
};

struct GitConfig {
    std::filesystem::path repo;
    bool auto_commit = true;
    std::string commit_template = "hyprsync: update from $hostname";
};

struct SshConfig {
    std::filesystem::path key;
    int port = 22;
    int timeout = 10;
};

struct HooksConfig {
    std::string pre_sync;
    std::string post_sync;
    std::unordered_map<std::string, std::string> group_hooks;
};

struct Config {
    std::string hostname;
    SyncMode mode = SyncMode::Bidirectional;
    ConflictStrategy conflict_strategy = ConflictStrategy::NewestWins;
    int poll_interval = 0;
    bool dry_run = false;
    std::string log_level = "info";

    GitConfig git;
    SshConfig ssh;

    std::vector<Device> devices;
    std::vector<SyncGroup> sync_groups;

    HooksConfig hooks;
};

std::string sync_mode_to_string(SyncMode mode);
SyncMode sync_mode_from_string(const std::string& str);

std::string conflict_strategy_to_string(ConflictStrategy strategy);
ConflictStrategy conflict_strategy_from_string(const std::string& str);

Config load_config(const std::filesystem::path& path);

void save_config(const Config& config, const std::filesystem::path& path);

std::filesystem::path get_default_config_path();

std::filesystem::path get_default_repo_path();

}
