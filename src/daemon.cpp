#include "daemon.hpp"

#include <spdlog/spdlog.h>
#include <csignal>
#include <iostream>

#ifdef WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace hyprsync {

namespace {
    std::atomic<bool>* g_running = nullptr;
    std::atomic<bool> g_reload_requested{false};

    void signal_handler(int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            if (g_running) {
                g_running->store(false);
            }
        } else if (sig == SIGHUP) {
            g_reload_requested.store(true);
        }
    }
}

Daemon::Daemon(Config config)
    : config_(std::move(config)) {
    git_ = std::make_unique<GitManager>(config_.git, config_.hostname);
    sync_ = std::make_unique<SyncEngine>(config_, *git_);
    watcher_ = std::make_unique<Watcher>(config_);
}

void Daemon::run() {
    setup_signal_handlers();

    if (!git_->is_initialized()) {
        spdlog::error("git repo not initialized");
        return;
    }

    spdlog::info("starting hyprsync daemon");
    spdlog::info("hostname: {}", config_.hostname);
    spdlog::info("watching {} sync groups", config_.sync_groups.size());

#ifdef WITH_SYSTEMD
    sd_notify(0, "READY=1");
#endif

    watcher_->start();

    while (running_.load()) {
        if (g_reload_requested.load()) {
            g_reload_requested.store(false);
            reload_config();
        }

        auto events = watcher_->poll_changes(std::chrono::milliseconds(1000));

        if (!events.empty()) {
            handle_changes(events);
        }

#ifdef WITH_SYSTEMD
        sd_notify(0, "WATCHDOG=1");
#endif
    }

    watcher_->stop();

#ifdef WITH_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    spdlog::info("hyprsync daemon stopped");
}

void Daemon::shutdown() {
    running_.store(false);
}

void Daemon::handle_changes(const std::vector<FileEvent>& events) {
    spdlog::debug("handling {} file events", events.size());

    git_->snapshot(config_.sync_groups);

    if (!git_->has_changes()) {
        spdlog::debug("no actual changes after snapshot");
        return;
    }

    std::string message = config_.git.commit_template;
    size_t pos = message.find("$hostname");
    if (pos != std::string::npos) {
        message.replace(pos, 9, config_.hostname);
    }

    if (config_.git.auto_commit) {
        git_->commit(message);
    }

    if (!config_.dry_run) {
        auto results = sync_->sync_all(false);
        for (const auto& r : results) {
            if (r.success) {
                spdlog::info("synced {} to {}", r.group_name, r.device_name);
            } else {
                spdlog::error("failed to sync {} to {}: {}",
                              r.group_name, r.device_name, r.error_message);
            }
        }
    }
}

void Daemon::reload_config() {
    spdlog::info("reloading configuration");

    try {
        config_ = load_config(get_default_config_path());
        git_ = std::make_unique<GitManager>(config_.git, config_.hostname);
        sync_ = std::make_unique<SyncEngine>(config_, *git_);

        watcher_->stop();
        watcher_ = std::make_unique<Watcher>(config_);
        watcher_->start();

        spdlog::info("configuration reloaded");
    } catch (const std::exception& e) {
        spdlog::error("failed to reload config: {}", e.what());
    }
}

void Daemon::setup_signal_handlers() {
    g_running = &running_;

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
}

}
