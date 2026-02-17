#include "config.hpp"
#include "util.hpp"

#include <toml++/toml.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hyprsync {

std::string sync_mode_to_string(SyncMode mode) {
    switch (mode) {
        case SyncMode::Push: return "push";
        case SyncMode::Pull: return "pull";
        case SyncMode::Bidirectional: return "bidirectional";
    }
    return "bidirectional";
}

SyncMode sync_mode_from_string(const std::string& str) {
    if (str == "push") return SyncMode::Push;
    if (str == "pull") return SyncMode::Pull;
    return SyncMode::Bidirectional;
}

std::string conflict_strategy_to_string(ConflictStrategy strategy) {
    switch (strategy) {
        case ConflictStrategy::NewestWins: return "newest_wins";
        case ConflictStrategy::Manual: return "manual";
        case ConflictStrategy::KeepBoth: return "keep_both";
    }
    return "newest_wins";
}

ConflictStrategy conflict_strategy_from_string(const std::string& str) {
    if (str == "manual") return ConflictStrategy::Manual;
    if (str == "keep_both") return ConflictStrategy::KeepBoth;
    return ConflictStrategy::NewestWins;
}

Config load_config(const std::filesystem::path& path) {
    auto expanded_path = expand_path(path);

    if (!file_exists(expanded_path)) {
        throw std::runtime_error("config file not found: " + expanded_path.string());
    }

    toml::table tbl;
    try {
        tbl = toml::parse_file(expanded_path.string());
    } catch (const toml::parse_error& e) {
        throw std::runtime_error("failed to parse config: " + std::string(e.what()));
    }

    Config config;

    if (auto general = tbl["general"].as_table()) {
        config.hostname = general->get("hostname")->value_or(get_hostname());
        config.mode = sync_mode_from_string(
            general->get("mode")->value_or(std::string("bidirectional"))
        );
        config.conflict_strategy = conflict_strategy_from_string(
            general->get("conflict_strategy")->value_or(std::string("newest_wins"))
        );
        config.poll_interval = general->get("poll_interval")->value_or(0);
        config.dry_run = general->get("dry_run")->value_or(false);
        config.log_level = general->get("log_level")->value_or(std::string("info"));
    }

    if (auto git = tbl["git"].as_table()) {
        std::string repo_str = git->get("repo")->value_or(
            std::string("~/.local/share/hyprsync")
        );
        config.git.repo = expand_path(repo_str);
        config.git.auto_commit = git->get("auto_commit")->value_or(true);
        config.git.commit_template = git->get("commit_template")->value_or(
            std::string("hyprsync: update from $hostname")
        );
    } else {
        config.git.repo = expand_path("~/.local/share/hyprsync");
    }

    if (auto ssh = tbl["ssh"].as_table()) {
        std::string key_str = ssh->get("key")->value_or(
            std::string("~/.ssh/id_ed25519")
        );
        config.ssh.key = expand_path(key_str);
        config.ssh.port = ssh->get("port")->value_or(22);
        config.ssh.timeout = ssh->get("timeout")->value_or(10);
    } else {
        config.ssh.key = expand_path("~/.ssh/id_ed25519");
    }

    if (auto devices = tbl["device"].as_array()) {
        for (const auto& dev : *devices) {
            if (auto dev_tbl = dev.as_table()) {
                Device device;
                device.name = dev_tbl->get("name")->value_or(std::string(""));
                device.host = dev_tbl->get("host")->value_or(std::string(""));
                device.user = dev_tbl->get("user")->value_or(std::string(""));
                device.port = dev_tbl->get("port")->value_or(config.ssh.port);

                std::string key_str = dev_tbl->get("key")->value_or(std::string(""));
                if (!key_str.empty()) {
                    device.key = expand_path(key_str).string();
                }

                if (!device.name.empty() && !device.host.empty()) {
                    config.devices.push_back(device);
                }
            }
        }
    }

    if (auto syncs = tbl["sync"].as_array()) {
        for (const auto& sync : *syncs) {
            if (auto sync_tbl = sync.as_table()) {
                SyncGroup group;
                group.name = sync_tbl->get("name")->value_or(std::string(""));

                if (auto paths = sync_tbl->get("paths")->as_array()) {
                    for (const auto& p : *paths) {
                        if (auto path_str = p.value<std::string>()) {
                            group.paths.push_back(expand_path(*path_str));
                        }
                    }
                }

                if (auto excludes = sync_tbl->get("exclude")->as_array()) {
                    for (const auto& e : *excludes) {
                        if (auto exc_str = e.value<std::string>()) {
                            group.exclude.push_back(*exc_str);
                        }
                    }
                }

                if (auto devs = sync_tbl->get("devices")->as_array()) {
                    for (const auto& d : *devs) {
                        if (auto dev_str = d.value<std::string>()) {
                            group.devices.push_back(*dev_str);
                        }
                    }
                }

                std::string remote = sync_tbl->get("remote_path")->value_or(std::string(""));
                if (!remote.empty()) {
                    group.remote_path = remote;
                }

                if (!group.name.empty() && !group.paths.empty()) {
                    config.sync_groups.push_back(group);
                }
            }
        }
    }

    if (auto hooks = tbl["hooks"].as_table()) {
        config.hooks.pre_sync = hooks->get("pre_sync")->value_or(std::string(""));
        config.hooks.post_sync = hooks->get("post_sync")->value_or(std::string(""));

        if (auto group_hooks = hooks->get("group")->as_table()) {
            for (const auto& [key, value] : *group_hooks) {
                if (auto hook_str = value.value<std::string>()) {
                    config.hooks.group_hooks[std::string(key)] = *hook_str;
                }
            }
        }
    }

    return config;
}

void save_config(const Config& config, const std::filesystem::path& path) {
    auto expanded_path = expand_path(path);

    auto parent = expanded_path.parent_path();
    if (!parent.empty() && !dir_exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(expanded_path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open config file for writing: " +
                                 expanded_path.string());
    }

    file << "# HyprSync Configuration\n";
    file << "# Generated by: hyprsync init\n\n";

    file << "[general]\n";
    file << "hostname = \"" << config.hostname << "\"\n";
    file << "mode = \"" << sync_mode_to_string(config.mode) << "\"\n";
    file << "conflict_strategy = \"" << conflict_strategy_to_string(config.conflict_strategy) << "\"\n";
    file << "poll_interval = " << config.poll_interval << "\n";
    file << "dry_run = " << (config.dry_run ? "true" : "false") << "\n";
    file << "log_level = \"" << config.log_level << "\"\n";
    file << "\n";

    file << "[git]\n";
    file << "repo = \"" << config.git.repo.string() << "\"\n";
    file << "auto_commit = " << (config.git.auto_commit ? "true" : "false") << "\n";
    file << "commit_template = \"" << config.git.commit_template << "\"\n";
    file << "\n";

    file << "[ssh]\n";
    file << "key = \"" << config.ssh.key.string() << "\"\n";
    file << "port = " << config.ssh.port << "\n";
    file << "timeout = " << config.ssh.timeout << "\n";
    file << "\n";

    for (const auto& device : config.devices) {
        file << "[[device]]\n";
        file << "name = \"" << device.name << "\"\n";
        file << "host = \"" << device.host << "\"\n";
        file << "user = \"" << device.user << "\"\n";
        file << "port = " << device.port << "\n";
        if (!device.key.empty()) {
            file << "key = \"" << device.key << "\"\n";
        }
        file << "\n";
    }

    for (const auto& group : config.sync_groups) {
        file << "[[sync]]\n";
        file << "name = \"" << group.name << "\"\n";
        file << "paths = [\n";
        for (const auto& p : group.paths) {
            file << "    \"" << p.string() << "\",\n";
        }
        file << "]\n";

        if (!group.exclude.empty()) {
            file << "exclude = [\n";
            for (const auto& e : group.exclude) {
                file << "    \"" << e << "\",\n";
            }
            file << "]\n";
        }

        if (!group.devices.empty()) {
            file << "devices = [";
            for (size_t i = 0; i < group.devices.size(); ++i) {
                if (i > 0) file << ", ";
                file << "\"" << group.devices[i] << "\"";
            }
            file << "]\n";
        }

        if (!group.remote_path.empty()) {
            file << "remote_path = \"" << group.remote_path << "\"\n";
        }
        file << "\n";
    }

    file << "[hooks]\n";
    file << "pre_sync = \"" << config.hooks.pre_sync << "\"\n";
    file << "post_sync = \"" << config.hooks.post_sync << "\"\n";
    file << "\n";

    if (!config.hooks.group_hooks.empty()) {
        file << "[hooks.group]\n";
        for (const auto& [group, hook] : config.hooks.group_hooks) {
            file << group << " = \"" << hook << "\"\n";
        }
        file << "\n";
    }

    file.close();
}

std::filesystem::path get_default_config_path() {
    return expand_path("~/.config/hypr/hyprsync.toml");
}

std::filesystem::path get_default_repo_path() {
    return expand_path("~/.local/share/hyprsync");
}

}
