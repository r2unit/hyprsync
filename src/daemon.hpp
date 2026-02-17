#pragma once

#include "config.hpp"
#include "git.hpp"
#include "watcher.hpp"
#include "sync.hpp"

#include <atomic>

namespace hyprsync {

class Daemon {
public:
    explicit Daemon(Config config);

    void run();
    void shutdown();

private:
    Config config_;
    std::unique_ptr<Watcher> watcher_;
    std::unique_ptr<SyncEngine> sync_;
    std::unique_ptr<GitManager> git_;
    std::atomic<bool> running_{true};

    void handle_changes(const std::vector<FileEvent>& events);
    void reload_config();
    void setup_signal_handlers();
};

}
