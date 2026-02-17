#include "sync.hpp"
#include "util.hpp"

#include <spdlog/spdlog.h>
#include <sstream>

namespace hyprsync {

SyncEngine::SyncEngine(const Config& config, GitManager& git)
    : config_(config)
    , git_(git) {
}

SyncResult SyncEngine::sync_group(const SyncGroup& group,
                                   const Device& device,
                                   bool dry_run) {
    SyncResult result;
    result.device_name = device.name;
    result.group_name = group.name;
    result.success = false;
    result.files_synced = 0;

    if (!group.devices.empty()) {
        bool device_allowed = false;
        for (const auto& d : group.devices) {
            if (d == device.name) {
                device_allowed = true;
                break;
            }
        }
        if (!device_allowed) {
            result.success = true;
            return result;
        }
    }

    auto rsync_cmd = build_rsync_cmd(device, dry_run);
    auto exec_result = exec(rsync_cmd);

    if (exec_result.success()) {
        result.success = true;

        if (!dry_run) {
            std::string restore_cmd = "hyprsync restore 2>/dev/null || true";
            auto remote_result = remote_exec(device, restore_cmd);
            if (!remote_result.success()) {
                spdlog::warn("remote restore may have failed on {}", device.name);
            }

            auto hook_it = config_.hooks.group_hooks.find(group.name);
            if (hook_it != config_.hooks.group_hooks.end() && !hook_it->second.empty()) {
                remote_exec(device, hook_it->second);
            }
        }
    } else {
        result.error_message = exec_result.stderr_output;
    }

    return result;
}

std::vector<SyncResult> SyncEngine::sync_all(bool dry_run) {
    std::vector<SyncResult> results;

    if (!config_.hooks.pre_sync.empty()) {
        exec(config_.hooks.pre_sync);
    }

    git_.snapshot(config_.sync_groups);

    if (config_.git.auto_commit && git_.has_changes()) {
        std::string message = config_.git.commit_template;
        size_t pos = message.find("$hostname");
        if (pos != std::string::npos) {
            message.replace(pos, 9, config_.hostname);
        }
        git_.commit(message);
    }

    for (const auto& device : config_.devices) {
        for (const auto& group : config_.sync_groups) {
            auto result = sync_group(group, device, dry_run);
            results.push_back(result);
        }
    }

    if (!config_.hooks.post_sync.empty()) {
        exec(config_.hooks.post_sync);
    }

    return results;
}

DiffResult SyncEngine::diff(const Device& device) {
    DiffResult result;
    result.device_name = device.name;

    auto local_files = git_.changed_files();
    result.local_changes = local_files;

    return result;
}

bool SyncEngine::ping(const Device& device) {
    std::string key = get_ssh_key(device);

    std::vector<std::string> args = {
        "ssh",
        "-i", key,
        "-p", std::to_string(device.port),
        "-o", "ConnectTimeout=" + std::to_string(config_.ssh.timeout),
        "-o", "BatchMode=yes",
        "-o", "StrictHostKeyChecking=accept-new",
        device.user + "@" + device.host,
        "echo ok"
    };

    auto result = exec(args);
    return result.success() && trim(result.stdout_output) == "ok";
}

std::vector<std::string> SyncEngine::build_rsync_cmd(const Device& device,
                                                      bool dry_run) {
    std::string key = get_ssh_key(device);

    std::ostringstream ssh_cmd_stream;
    ssh_cmd_stream << "ssh -i " << key
                   << " -p " << device.port
                   << " -o ConnectTimeout=" << config_.ssh.timeout
                   << " -o StrictHostKeyChecking=accept-new";

    std::vector<std::string> cmd = {
        "rsync",
        "-avz",
        "--checksum",
        "--partial",
        "--delete",
        "--exclude=.git/",
        "-e", ssh_cmd_stream.str(),
    };

    if (dry_run) {
        cmd.push_back("--dry-run");
    }

    std::string source = git_.repo_path().string();
    if (source.back() != '/') {
        source += '/';
    }
    cmd.push_back(source);

    std::string dest = device.user + "@" + device.host + ":"
                     + config_.git.repo.string() + "/";
    cmd.push_back(dest);

    return cmd;
}

ExecResult SyncEngine::remote_exec(const Device& device,
                                    const std::string& command) {
    std::string key = get_ssh_key(device);

    std::vector<std::string> args = {
        "ssh",
        "-i", key,
        "-p", std::to_string(device.port),
        "-o", "ConnectTimeout=" + std::to_string(config_.ssh.timeout),
        "-o", "BatchMode=yes",
        "-o", "StrictHostKeyChecking=accept-new",
        device.user + "@" + device.host,
        command
    };

    return exec(args);
}

std::string SyncEngine::ssh_cmd(const Device& device) {
    std::string key = get_ssh_key(device);

    std::ostringstream cmd;
    cmd << "ssh -i " << key
        << " -p " << device.port
        << " -o ConnectTimeout=" << config_.ssh.timeout
        << " -o StrictHostKeyChecking=accept-new";

    return cmd.str();
}

std::string SyncEngine::get_ssh_key(const Device& device) {
    if (!device.key.empty()) {
        return device.key;
    }
    return config_.ssh.key.string();
}

}
