#pragma once

#include "config.hpp"
#include "git.hpp"

#include <string>
#include <vector>

namespace hyprsync {

struct SyncResult {
    bool success;
    std::string device_name;
    std::string group_name;
    int files_synced;
    std::string error_message;
};

struct DiffResult {
    std::string device_name;
    std::vector<std::string> local_changes;
    std::vector<std::string> remote_changes;
};

class SyncEngine {
public:
    SyncEngine(const Config& config, GitManager& git);

    SyncResult sync_group(const SyncGroup& group, const Device& device, bool dry_run);
    std::vector<SyncResult> sync_all(bool dry_run);

    DiffResult diff(const Device& device);

    bool ping(const Device& device);

private:
    const Config& config_;
    GitManager& git_;

    std::vector<std::string> build_rsync_cmd(const Device& device, bool dry_run);
    ExecResult remote_exec(const Device& device, const std::string& command);
    std::string ssh_cmd(const Device& device);
    std::string get_ssh_key(const Device& device);
};

}
