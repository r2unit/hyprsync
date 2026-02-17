#pragma once

#include "config.hpp"
#include "tui.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace hyprsync {

class SetupWizard {
public:
    SetupWizard();

    Config run();

private:
    Tui tui_;

    std::string prompt_hostname();
    std::pair<std::filesystem::path, int> prompt_ssh();
    std::vector<Device> prompt_devices(const std::filesystem::path& key, int port);
    std::vector<SyncGroup> prompt_sync_paths();
    SyncMode prompt_mode();
    ConflictStrategy prompt_conflict_strategy();

    std::vector<std::filesystem::path> scan_config_dirs();
    std::vector<std::filesystem::path> scan_dotfiles();
    std::vector<std::filesystem::path> scan_local_dirs();
    std::vector<std::filesystem::path> scan_ssh_keys();

    bool test_connection(const Device& device, const std::filesystem::path& key);

    void write_config(const Config& config, const std::filesystem::path& path);
    void init_git_repo(const Config& config);
    void print_summary(const Config& config);
    void print_next_steps();
};

}
