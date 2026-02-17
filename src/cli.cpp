#include "cli.hpp"
#include "daemon.hpp"
#include "setup.hpp"
#include "git.hpp"
#include "sync.hpp"
#include "upgrade.hpp"
#include "util.hpp"

#include <hyprsync/version.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

namespace hyprsync {

Cli::Cli(int argc, char* argv[]) {
    parse_args(argc, argv);
}

void Cli::parse_args(int argc, char* argv[]) {
    options_.config_path = get_default_config_path();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                options_.config_path = expand_path(argv[++i]);
            }
        } else if (arg == "-n" || arg == "--dry-run") {
            options_.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            options_.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            options_.quiet = true;
        } else if (arg == "-g" || arg == "--group") {
            if (i + 1 < argc) {
                options_.group = argv[++i];
            }
        } else if (arg == "-d" || arg == "--device") {
            if (i + 1 < argc) {
                options_.device = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            options_.command = "help";
        } else if (arg[0] != '-') {
            if (options_.command.empty()) {
                options_.command = arg;
            } else {
                options_.args.push_back(arg);
            }
        }
    }

    if (options_.command.empty()) {
        options_.command = "help";
    }
}

void Cli::load_config() {
    if (config_.has_value()) {
        return;
    }

    try {
        config_ = hyprsync::load_config(options_.config_path);

        if (options_.dry_run) {
            config_->dry_run = true;
        }

        if (options_.verbose) {
            spdlog::set_level(spdlog::level::debug);
        } else if (options_.quiet) {
            spdlog::set_level(spdlog::level::err);
        } else {
            if (config_->log_level == "debug") {
                spdlog::set_level(spdlog::level::debug);
            } else if (config_->log_level == "warn") {
                spdlog::set_level(spdlog::level::warn);
            } else if (config_->log_level == "error") {
                spdlog::set_level(spdlog::level::err);
            } else {
                spdlog::set_level(spdlog::level::info);
            }
        }
    } catch (const std::exception& e) {
        spdlog::debug("could not load config: {}", e.what());
    }
}

int Cli::run() {
    if (options_.command == "init") {
        return cmd_init();
    } else if (options_.command == "daemon") {
        return cmd_daemon();
    } else if (options_.command == "sync") {
        return cmd_sync();
    } else if (options_.command == "status") {
        return cmd_status();
    } else if (options_.command == "diff") {
        return cmd_diff();
    } else if (options_.command == "log") {
        return cmd_log();
    } else if (options_.command == "ping") {
        return cmd_ping();
    } else if (options_.command == "conflicts") {
        return cmd_conflicts();
    } else if (options_.command == "restore") {
        return cmd_restore();
    } else if (options_.command == "upgrade") {
        return cmd_upgrade();
    } else if (options_.command == "version") {
        return cmd_version();
    } else if (options_.command == "help") {
        return cmd_help();
    }

    std::cerr << "unknown command: " << options_.command << "\n";
    print_usage();
    return 1;
}

int Cli::cmd_init() {
    try {
        SetupWizard wizard;
        wizard.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "setup failed: " << e.what() << "\n";
        return 1;
    }
}

int Cli::cmd_daemon() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    Daemon daemon(*config_);
    daemon.run();
    return 0;
}

int Cli::cmd_sync() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized. run 'hyprsync init' first.\n";
        return 1;
    }

    SyncEngine sync(*config_, git);

    bool dry_run = options_.dry_run || config_->dry_run;

    if (dry_run) {
        std::cout << "dry-run mode: showing what would be synced\n\n";
    }

    if (!options_.group.empty() && !options_.device.empty()) {
        const SyncGroup* group = nullptr;
        const Device* device = nullptr;

        for (const auto& g : config_->sync_groups) {
            if (g.name == options_.group) {
                group = &g;
                break;
            }
        }

        for (const auto& d : config_->devices) {
            if (d.name == options_.device) {
                device = &d;
                break;
            }
        }

        if (!group) {
            std::cerr << "unknown group: " << options_.group << "\n";
            return 1;
        }

        if (!device) {
            std::cerr << "unknown device: " << options_.device << "\n";
            return 1;
        }

        auto result = sync.sync_group(*group, *device, dry_run);

        if (result.success) {
            std::cout << "synced " << result.group_name << " to "
                      << result.device_name << "\n";
        } else {
            std::cerr << "failed to sync " << result.group_name << " to "
                      << result.device_name << ": " << result.error_message << "\n";
            return 1;
        }
    } else {
        auto results = sync.sync_all(dry_run);

        int failures = 0;
        for (const auto& r : results) {
            if (r.success) {
                if (r.files_synced > 0 || !options_.quiet) {
                    std::cout << "synced " << r.group_name << " to "
                              << r.device_name << "\n";
                }
            } else {
                std::cerr << "failed: " << r.group_name << " to "
                          << r.device_name << ": " << r.error_message << "\n";
                failures++;
            }
        }

        if (failures > 0) {
            return 1;
        }
    }

    if (!dry_run) {
        std::cout << "\nsync complete\n";
    }

    return 0;
}

int Cli::cmd_status() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized. run 'hyprsync init' first.\n";
        return 1;
    }

    std::cout << "hyprsync status\n";
    std::cout << "  hostname: " << config_->hostname << "\n";
    std::cout << "  mode: " << sync_mode_to_string(config_->mode) << "\n";
    std::cout << "  devices: " << config_->devices.size() << "\n";
    std::cout << "  sync groups: " << config_->sync_groups.size() << "\n";
    std::cout << "\n";

    if (git.has_changes()) {
        std::cout << "  local changes pending:\n";
        auto files = git.changed_files();
        for (const auto& f : files) {
            std::cout << "    " << f << "\n";
        }
    } else {
        std::cout << "  no local changes\n";
    }

    return 0;
}

int Cli::cmd_diff() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized. run 'hyprsync init' first.\n";
        return 1;
    }

    std::string diff_output;

    if (!options_.args.empty()) {
        diff_output = git.diff_remote(options_.args[0]);
    } else {
        diff_output = git.diff();
    }

    if (diff_output.empty()) {
        std::cout << "no changes\n";
    } else {
        std::cout << diff_output;
    }

    return 0;
}

int Cli::cmd_log() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized. run 'hyprsync init' first.\n";
        return 1;
    }

    auto entries = git.log(20);

    if (entries.empty()) {
        std::cout << "no sync history\n";
    } else {
        std::cout << "recent sync history:\n";
        for (const auto& entry : entries) {
            std::cout << "  " << entry << "\n";
        }
    }

    return 0;
}

int Cli::cmd_ping() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    if (config_->devices.empty()) {
        std::cout << "no devices configured\n";
        return 0;
    }

    std::cout << "testing device connectivity...\n\n";

    int failures = 0;

    for (const auto& device : config_->devices) {
        std::cout << "  " << device.name << " (" << device.user << "@"
                  << device.host << ":" << device.port << "): ";
        std::cout.flush();

        std::string key = device.key.empty()
            ? config_->ssh.key.string()
            : device.key;

        std::vector<std::string> args = {
            "ssh",
            "-i", key,
            "-p", std::to_string(device.port),
            "-o", "ConnectTimeout=" + std::to_string(config_->ssh.timeout),
            "-o", "BatchMode=yes",
            "-o", "StrictHostKeyChecking=accept-new",
            device.user + "@" + device.host,
            "echo ok"
        };

        auto result = exec(args);

        if (result.success() && trim(result.stdout_output) == "ok") {
            std::cout << "OK\n";
        } else {
            std::cout << "FAILED\n";
            failures++;
        }
    }

    std::cout << "\n";

    if (failures == 0) {
        std::cout << "all devices reachable\n";
    } else {
        std::cout << failures << " device(s) unreachable\n";
    }

    return failures > 0 ? 1 : 0;
}

int Cli::cmd_conflicts() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized. run 'hyprsync init' first.\n";
        return 1;
    }

    auto conflicts = git.get_conflicts();

    if (conflicts.empty()) {
        std::cout << "no conflicts\n";
        return 0;
    }

    std::cout << "conflicts:\n";
    for (const auto& c : conflicts) {
        std::cout << "  " << c.path.string() << "\n";
    }

    return 0;
}

int Cli::cmd_restore() {
    load_config();

    if (!config_.has_value()) {
        std::cerr << "no config found. run 'hyprsync init' first.\n";
        return 1;
    }

    GitManager git(config_->git, config_->hostname);

    if (!git.is_initialized()) {
        std::cerr << "git repo not initialized.\n";
        return 1;
    }

    git.restore(config_->sync_groups);
    std::cout << "files restored from repo\n";

    return 0;
}

int Cli::cmd_upgrade() {
    Upgrader upgrader;

    if (options_.args.empty()) {
        return upgrader.upgrade_to_latest() ? 0 : 1;
    }

    std::string arg = options_.args[0];

    if (arg == "list" || arg == "--list" || arg == "-l") {
        upgrader.list_available_versions();
        return 0;
    }

    if (arg == "check" || arg == "--check") {
        if (upgrader.has_update()) {
            auto latest = upgrader.get_latest_release();
            if (latest.has_value()) {
                std::cout << "update available: " << latest->version.to_string() << "\n";
                std::cout << "current version: " << upgrader.current_version().to_string() << "\n";
            }
            return 0;
        } else {
            std::cout << "already running the latest version\n";
            return 0;
        }
    }

    return upgrader.upgrade_to_version(arg) ? 0 : 1;
}

int Cli::cmd_version() {
    std::cout << "hyprsync " << VERSION << "\n";
    std::cout << "  build: " << BUILD_DATE << " (" << GIT_COMMIT << ")\n";
    return 0;
}

int Cli::cmd_help() {
    print_usage();
    return 0;
}

void Cli::print_usage() const {
    std::cout << "hyprsync - a lightweight sync daemon for Hyprland users\n";
    std::cout << "\n";
    std::cout << "usage:\n";
    std::cout << "    hyprsync <command> [options]\n";
    std::cout << "\n";
    std::cout << "commands:\n";
    std::cout << "    init              interactive setup wizard\n";
    std::cout << "    daemon            start the sync daemon\n";
    std::cout << "    sync              run a one-shot sync\n";
    std::cout << "    restore           restore files from repo to original locations\n";
    std::cout << "    status            show sync status\n";
    std::cout << "    diff [device]     show pending changes\n";
    std::cout << "    log               show sync history\n";
    std::cout << "    ping              test device connectivity\n";
    std::cout << "    conflicts         list sync conflicts\n";
    std::cout << "    upgrade [version] upgrade to latest or specific version\n";
    std::cout << "    upgrade list      list available versions\n";
    std::cout << "    upgrade check     check for updates\n";
    std::cout << "    version           show version info\n";
    std::cout << "\n";
    std::cout << "options:\n";
    std::cout << "    -c, --config <path>   config file path\n";
    std::cout << "    -n, --dry-run         show what would be synced\n";
    std::cout << "    -v, --verbose         enable verbose output\n";
    std::cout << "    -q, --quiet           suppress non-error output\n";
    std::cout << "    -g, --group <name>    only sync a specific group\n";
    std::cout << "    -d, --device <name>   only sync with a specific device\n";
    std::cout << "    -h, --help            show this help\n";
    std::cout << "\n";
    std::cout << "examples:\n";
    std::cout << "    hyprsync init\n";
    std::cout << "    hyprsync sync --dry-run\n";
    std::cout << "    hyprsync sync -g hyprland -d desktop\n";
    std::cout << "    hyprsync upgrade\n";
    std::cout << "    hyprsync upgrade 2026.2.1\n";
    std::cout << "\n";
}

}
