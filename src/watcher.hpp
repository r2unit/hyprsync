#pragma once

#include "config.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hyprsync {

enum class FileEventType {
    Created,
    Modified,
    Deleted,
    Moved
};

struct FileEvent {
    FileEventType type;
    std::filesystem::path path;
    std::chrono::steady_clock::time_point timestamp;
};

class Watcher {
public:
    explicit Watcher(const Config& config);
    ~Watcher();

    void start();
    void stop();

    std::vector<FileEvent> poll_changes(std::chrono::milliseconds timeout);

private:
    const Config& config_;
    int inotify_fd_ = -1;
    std::unordered_map<int, std::filesystem::path> watch_descriptors_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<FileEvent> pending_events_;
    std::atomic<bool> running_{false};
    std::thread watch_thread_;

    void add_watch_recursive(const std::filesystem::path& path);
    void remove_watch(int wd);
    void watch_loop();
    void process_event(int wd, uint32_t mask, const char* name);
};

}
