#include "setup.hpp"
#include "git.hpp"
#include "util.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <iostream>

namespace hyprsync {

namespace {

const std::vector<std::string> CONFIG_SCAN = {
    "hypr", "waybar", "wofi", "dunst", "rofi", "kitty",
    "alacritty", "nvim", "neovim", "foot", "sway", "i3",
    "polybar", "picom", "zsh", "fish", "tmux", "wezterm",
    "mako", "swaylock", "hyprlock", "hypridle", "hyprpaper",
    "starship.toml", "fastfetch",
};

const std::vector<std::string> HOME_SCAN = {
    ".zshrc", ".zshenv", ".bashrc", ".bash_profile",
    ".gitconfig", ".tmux.conf", ".vimrc", ".nanorc",
};

const std::vector<std::string> LOCAL_SCAN = {
    "bin",
};

const std::vector<std::string> HYPRLAND_ITEMS = {
    "hypr", "waybar", "dunst", "hyprlock", "hypridle", "hyprpaper",
};

bool is_hyprland_item(const std::string& name) {
    return std::find(HYPRLAND_ITEMS.begin(), HYPRLAND_ITEMS.end(), name)
           != HYPRLAND_ITEMS.end();
}

}

SetupWizard::SetupWizard() {
}

Config SetupWizard::run() {
    Config config;

    tui_.print_header("HyprSync Setup");

    tui_.print_step(1, "Machine Identity");
    config.hostname = prompt_hostname();
    tui_.print_blank();

    tui_.print_step(2, "SSH Configuration");
    auto [ssh_key, ssh_port] = prompt_ssh();
    config.ssh.key = ssh_key;
    config.ssh.port = ssh_port;
    tui_.print_blank();

    tui_.print_step(3, "Devices");
    config.devices = prompt_devices(config.ssh.key, config.ssh.port);
    tui_.print_blank();

    tui_.print_step(4, "Select paths to sync");
    config.sync_groups = prompt_sync_paths();
    tui_.print_blank();

    tui_.print_step(5, "Sync mode");
    config.mode = prompt_mode();
    tui_.print_blank();

    tui_.print_step(6, "Conflict resolution");
    config.conflict_strategy = prompt_conflict_strategy();
    tui_.print_blank();

    config.git.repo = get_default_repo_path();
    config.git.auto_commit = true;
    config.git.commit_template = "hyprsync: update from $hostname";

    tui_.print_line();
    print_summary(config);
    tui_.print_line();

    auto config_path = get_default_config_path();
    std::string path_display = "~/.config/hypr/hyprsync.toml";

    if (tui_.confirm("Write config to " + path_display + "?", true)) {
        write_config(config, config_path);
        tui_.print_success("Config written");

        init_git_repo(config);
        tui_.print_success("Git repository initialized");

        tui_.print_blank();
        print_next_steps();
    } else {
        tui_.print_info("Setup cancelled");
    }

    return config;
}

std::string SetupWizard::prompt_hostname() {
    std::string detected = get_hostname();
    return tui_.prompt("Hostname for this machine", detected);
}

std::pair<std::filesystem::path, int> SetupWizard::prompt_ssh() {
    auto keys = scan_ssh_keys();

    std::filesystem::path selected_key;
    if (!keys.empty()) {
        std::vector<std::string> key_names;
        int default_idx = 0;

        for (size_t i = 0; i < keys.size(); ++i) {
            key_names.push_back(keys[i].filename().string());
            if (keys[i].filename() == "id_ed25519") {
                default_idx = static_cast<int>(i);
            }
        }

        tui_.print_info("Found SSH keys:");
        int idx = tui_.select("Select SSH key", key_names, default_idx);
        selected_key = keys[idx];
    } else {
        std::string key_path = tui_.prompt("SSH key path", "~/.ssh/id_ed25519");
        selected_key = expand_path(key_path);
    }

    std::string port_str = tui_.prompt("Default SSH port", "22");
    int port = 22;
    try {
        port = std::stoi(port_str);
    } catch (...) {
        port = 22;
    }

    return {selected_key, port};
}

std::vector<Device> SetupWizard::prompt_devices(const std::filesystem::path& key,
                                                  int default_port) {
    std::vector<Device> devices;

    bool add_more = true;
    while (add_more) {
        tui_.print_info("Add a device to sync with:");

        Device device;
        device.name = tui_.prompt("  Name");

        if (device.name.empty()) {
            break;
        }

        device.host = tui_.prompt("  Host (IP or hostname)");
        device.user = tui_.prompt("  User");

        std::string port_str = tui_.prompt("  Port", std::to_string(default_port));
        try {
            device.port = std::stoi(port_str);
        } catch (...) {
            device.port = default_port;
        }

        tui_.print_info("  Testing connection...");
        if (test_connection(device, key)) {
            tui_.print_success("Connection OK");
            devices.push_back(device);
        } else {
            tui_.print_error("Connection failed");
            if (tui_.confirm("  Add device anyway?", false)) {
                devices.push_back(device);
            }
        }

        add_more = tui_.confirm("Add another device?", false);
    }

    return devices;
}

std::vector<SyncGroup> SetupWizard::prompt_sync_paths() {
    tui_.print_info("Scanning for dotfiles...");
    tui_.print_blank();

    auto config_dirs = scan_config_dirs();
    auto dotfiles = scan_dotfiles();
    auto local_dirs = scan_local_dirs();

    std::vector<SyncGroup> groups;

    if (!config_dirs.empty()) {
        tui_.print_info("Config directories:");

        std::vector<std::string> items;
        std::vector<bool> defaults;

        for (const auto& dir : config_dirs) {
            std::string name = dir.filename().string();
            items.push_back("~/.config/" + name + "/");
            defaults.push_back(is_hyprland_item(name));
        }

        auto selected = tui_.checkbox("Select directories", items, defaults);

        if (!selected.empty()) {
            SyncGroup group;
            group.name = "config";
            for (size_t idx : selected) {
                group.paths.push_back(config_dirs[idx]);
            }
            group.exclude = {"*.log", "*.bak", "__pycache__/"};
            groups.push_back(group);
        }
    }

    if (!dotfiles.empty()) {
        tui_.print_blank();
        tui_.print_info("Dotfiles:");

        std::vector<std::string> items;
        std::vector<bool> defaults;

        for (const auto& file : dotfiles) {
            items.push_back("~/" + file.filename().string());
            defaults.push_back(true);
        }

        auto selected = tui_.checkbox("Select dotfiles", items, defaults);

        if (!selected.empty()) {
            SyncGroup group;
            group.name = "dotfiles";
            for (size_t idx : selected) {
                group.paths.push_back(dotfiles[idx]);
            }
            groups.push_back(group);
        }
    }

    if (!local_dirs.empty()) {
        tui_.print_blank();
        tui_.print_info("Local directories:");

        std::vector<std::string> items;
        std::vector<bool> defaults;

        for (const auto& dir : local_dirs) {
            items.push_back("~/.local/" + dir.filename().string() + "/");
            defaults.push_back(true);
        }

        auto selected = tui_.checkbox("Select directories", items, defaults);

        if (!selected.empty()) {
            SyncGroup group;
            group.name = "local";
            for (size_t idx : selected) {
                group.paths.push_back(local_dirs[idx]);
            }
            group.exclude = {"__pycache__/", "*.pyc"};
            groups.push_back(group);
        }
    }

    tui_.print_blank();
    if (tui_.confirm("Add a custom path?", false)) {
        SyncGroup custom_group;
        custom_group.name = "custom";

        bool add_more = true;
        while (add_more) {
            std::string path = tui_.prompt("  Path");
            if (!path.empty()) {
                custom_group.paths.push_back(expand_path(path));
            }
            add_more = !path.empty() && tui_.confirm("Add another?", false);
        }

        if (!custom_group.paths.empty()) {
            groups.push_back(custom_group);
        }
    }

    return groups;
}

SyncMode SetupWizard::prompt_mode() {
    std::vector<std::string> options = {
        "bidirectional - sync changes both ways",
        "push - only push local changes to remotes",
        "pull - only pull changes from remotes"
    };

    int idx = tui_.select("Sync mode", options, 0);

    switch (idx) {
        case 1: return SyncMode::Push;
        case 2: return SyncMode::Pull;
        default: return SyncMode::Bidirectional;
    }
}

ConflictStrategy SetupWizard::prompt_conflict_strategy() {
    std::vector<std::string> options = {
        "newest_wins - file with latest modification time wins",
        "manual - pause and ask for resolution",
        "keep_both - save both versions with hostname suffix"
    };

    int idx = tui_.select("Conflict resolution strategy", options, 0);

    switch (idx) {
        case 1: return ConflictStrategy::Manual;
        case 2: return ConflictStrategy::KeepBoth;
        default: return ConflictStrategy::NewestWins;
    }
}

std::vector<std::filesystem::path> SetupWizard::scan_config_dirs() {
    std::vector<std::filesystem::path> found;
    auto config_dir = expand_path("~/.config");

    if (!dir_exists(config_dir)) {
        return found;
    }

    for (const auto& name : CONFIG_SCAN) {
        auto path = config_dir / name;
        if (dir_exists(path) || file_exists(path)) {
            found.push_back(path);
        }
    }

    return found;
}

std::vector<std::filesystem::path> SetupWizard::scan_dotfiles() {
    std::vector<std::filesystem::path> found;
    auto home = get_home_dir();

    for (const auto& name : HOME_SCAN) {
        auto path = home / name;
        if (file_exists(path)) {
            found.push_back(path);
        }
    }

    return found;
}

std::vector<std::filesystem::path> SetupWizard::scan_local_dirs() {
    std::vector<std::filesystem::path> found;
    auto local_dir = expand_path("~/.local");

    if (!dir_exists(local_dir)) {
        return found;
    }

    for (const auto& name : LOCAL_SCAN) {
        auto path = local_dir / name;
        if (dir_exists(path)) {
            found.push_back(path);
        }
    }

    return found;
}

std::vector<std::filesystem::path> SetupWizard::scan_ssh_keys() {
    std::vector<std::filesystem::path> found;
    auto ssh_dir = expand_path("~/.ssh");

    if (!dir_exists(ssh_dir)) {
        return found;
    }

    std::vector<std::string> key_names = {
        "id_ed25519", "id_rsa", "id_ecdsa", "id_dsa"
    };

    for (const auto& name : key_names) {
        auto path = ssh_dir / name;
        if (file_exists(path)) {
            found.push_back(path);
        }
    }

    return found;
}

bool SetupWizard::test_connection(const Device& device,
                                   const std::filesystem::path& key) {
    std::vector<std::string> args = {
        "ssh",
        "-i", key.string(),
        "-p", std::to_string(device.port),
        "-o", "ConnectTimeout=5",
        "-o", "BatchMode=yes",
        "-o", "StrictHostKeyChecking=accept-new",
        device.user + "@" + device.host,
        "echo ok"
    };

    auto result = exec(args);
    return result.success() && trim(result.stdout_output) == "ok";
}

void SetupWizard::write_config(const Config& config,
                                const std::filesystem::path& path) {
    save_config(config, path);
}

void SetupWizard::init_git_repo(const Config& config) {
    GitManager git(config.git, config.hostname);

    if (!git.init_repo()) {
        throw std::runtime_error("failed to initialize git repository");
    }

    git.snapshot(config.sync_groups);

    std::string message = "hyprsync: initial snapshot from " + config.hostname;
    git.commit(message);
}

void SetupWizard::print_summary(const Config& config) {
    tui_.print_blank();
    tui_.print_info("Summary:");
    tui_.print_blank();
    tui_.print_info("  Machine:    " + config.hostname);

    std::string devices_str;
    for (size_t i = 0; i < config.devices.size(); ++i) {
        if (i > 0) devices_str += ", ";
        const auto& d = config.devices[i];
        devices_str += d.name + " (" + d.user + "@" + d.host + ")";
    }
    if (devices_str.empty()) devices_str = "(none)";
    tui_.print_info("  Devices:    " + devices_str);

    size_t total_paths = 0;
    for (const auto& g : config.sync_groups) {
        total_paths += g.paths.size();
    }
    tui_.print_info("  Syncing:    " + std::to_string(total_paths) + " paths in " +
                    std::to_string(config.sync_groups.size()) + " groups");

    tui_.print_info("  Mode:       " + sync_mode_to_string(config.mode));
    tui_.print_info("  Conflicts:  " + conflict_strategy_to_string(config.conflict_strategy));
    tui_.print_info("  Git repo:   ~/.local/share/hyprsync/");
    tui_.print_blank();
}

void SetupWizard::print_next_steps() {
    tui_.print_info("Next steps:");
    tui_.print_info("  hyprsync ping          # Verify device connectivity");
    tui_.print_info("  hyprsync sync          # First sync");
    tui_.print_info("  hyprsync daemon        # Start background daemon");
    tui_.print_blank();
}

}
