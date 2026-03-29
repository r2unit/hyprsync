#pragma once

#include "config.hpp"

#include <optional>
#include <string>
#include <vector>

namespace hyprsync {

struct CliOptions {
    std::string command;
    std::filesystem::path config_path;
    bool dry_run = false;
    bool verbose = false;
    bool quiet = false;
    bool devel = false;
    std::string group;
    std::string device;
    std::vector<std::string> args;
};

class Cli {
public:
    Cli(int argc, char* argv[]);

    int run();

private:
    CliOptions options_;
    std::optional<Config> config_;

    void parse_args(int argc, char* argv[]);
    void load_config();

    int cmd_init();
    int cmd_daemon();
    int cmd_sync();
    int cmd_status();
    int cmd_diff();
    int cmd_log();
    int cmd_ping();
    int cmd_conflicts();
    int cmd_restore();
    int cmd_upgrade();
    int cmd_version();
    int cmd_help();

    bool has_flag(const std::string& flag) const;
    std::string get_option(const std::string& flag,
                           const std::string& default_val = "") const;

    void print_usage() const;
};

}
