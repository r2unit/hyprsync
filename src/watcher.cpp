#include "watcher.hpp"
#include "util.hpp"

#include <spdlog/spdlog.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>

namespace hyprsync {

namespace {
    constexpr size_t EVENT_BUF_LEN = 4096;
    constexpr auto DEBOUNCE_MS = std::chrono::milliseconds(500);
}

Watcher::Watcher(const Config& config)
    : config_(config) {
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ < 0) {
        spdlog::error("failed to initialize inotify: {}", strerror(errno));
    }
}

Watcher::~Watcher() {
    stop();
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
    }
}

void Watcher::start() {
    if (running_.load() || inotify_fd_ < 0) {
        return;
    }

    running_.store(true);

    for (const auto& group : config_.sync_groups) {
        for (const auto& path : group.paths) {
            auto expanded = expand_path(path);
            if (dir_exists(expanded) || file_exists(expanded)) {
                add_watch_recursive(expanded);
            }
        }
    }

    watch_thread_ = std::thread(&Watcher::watch_loop, this);

    spdlog::info("watcher started with {} watches", watch_descriptors_.size());
}

void Watcher::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    queue_cv_.notify_all();

    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }

    for (const auto& [wd, _] : watch_descriptors_) {
        inotify_rm_watch(inotify_fd_, wd);
    }
    watch_descriptors_.clear();

    spdlog::debug("watcher stopped");
}

std::vector<FileEvent> Watcher::poll_changes(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (pending_events_.empty()) {
        queue_cv_.wait_for(lock, timeout);
    }

    std::vector<FileEvent> events;
    std::swap(events, pending_events_);

    return events;
}

void Watcher::add_watch_recursive(const std::filesystem::path& path) {
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    if (std::filesystem::is_directory(path)) {
        int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
        if (wd >= 0) {
            watch_descriptors_[wd] = path;
            spdlog::debug("watching directory: {}", path.string());
        } else {
            spdlog::warn("failed to watch {}: {}", path.string(), strerror(errno));
        }

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name != ".git" && name[0] != '.') {
                    add_watch_recursive(entry.path());
                }
            }
        }
    } else if (std::filesystem::is_regular_file(path)) {
        auto parent = path.parent_path();
        int wd = inotify_add_watch(inotify_fd_, parent.c_str(), mask);
        if (wd >= 0) {
            watch_descriptors_[wd] = parent;
            spdlog::debug("watching file's parent: {}", parent.string());
        }
    }
}

void Watcher::remove_watch(int wd) {
    auto it = watch_descriptors_.find(wd);
    if (it != watch_descriptors_.end()) {
        watch_descriptors_.erase(it);
    }
}

void Watcher::watch_loop() {
    char buffer[EVENT_BUF_LEN];
    auto last_event_time = std::chrono::steady_clock::now();

    while (running_.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(inotify_fd_, &fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(inotify_fd_ + 1, &fds, nullptr, nullptr, &tv);

        if (ret < 0) {
            if (errno != EINTR) {
                spdlog::error("select error: {}", strerror(errno));
            }
            continue;
        }

        if (ret == 0) {
            auto now = std::chrono::steady_clock::now();
            if (!pending_events_.empty() &&
                (now - last_event_time) > DEBOUNCE_MS) {
                queue_cv_.notify_one();
            }
            continue;
        }

        ssize_t len = read(inotify_fd_, buffer, EVENT_BUF_LEN);
        if (len < 0) {
            if (errno != EAGAIN) {
                spdlog::error("read error: {}", strerror(errno));
            }
            continue;
        }

        ssize_t i = 0;
        while (i < len) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

            if (event->mask & IN_Q_OVERFLOW) {
                spdlog::warn("inotify queue overflow, triggering full rescan");
            } else {
                const char* name = event->len > 0 ? event->name : "";
                process_event(event->wd, event->mask, name);
                last_event_time = std::chrono::steady_clock::now();
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }
}

void Watcher::process_event(int wd, uint32_t mask, const char* name) {
    auto it = watch_descriptors_.find(wd);
    if (it == watch_descriptors_.end()) {
        return;
    }

    std::filesystem::path path = it->second;
    if (name && strlen(name) > 0) {
        path /= name;
    }

    std::string filename = path.filename().string();
    if (filename.rfind(".git", 0) == 0) {
        return;
    }

    FileEvent event;
    event.path = path;
    event.timestamp = std::chrono::steady_clock::now();

    if (mask & IN_CREATE) {
        event.type = FileEventType::Created;
        spdlog::debug("file created: {}", path.string());

        if (std::filesystem::is_directory(path)) {
            add_watch_recursive(path);
        }
    } else if (mask & IN_MODIFY) {
        event.type = FileEventType::Modified;
        spdlog::debug("file modified: {}", path.string());
    } else if (mask & IN_DELETE) {
        event.type = FileEventType::Deleted;
        spdlog::debug("file deleted: {}", path.string());
    } else if (mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
        event.type = FileEventType::Moved;
        spdlog::debug("file moved: {}", path.string());
    } else {
        return;
    }

    if (mask & IN_IGNORED) {
        remove_watch(wd);
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_events_.push_back(event);
}

}
