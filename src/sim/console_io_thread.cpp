#include "nb/console_io_thread.hpp"

#include <chrono>
#include <iostream>

namespace nb {

ConsoleIoThread::ConsoleIoThread() = default;

ConsoleIoThread::~ConsoleIoThread() { stop(); }

void ConsoleIoThread::start() {
    if (started_) return;
    started_ = true;
    thread_ = std::thread(&ConsoleIoThread::run, this);
}

void ConsoleIoThread::stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.detach();  // stdin read cannot be interrupted portably.
    }
}

std::optional<std::string> ConsoleIoThread::waitForLine(long timeoutMs) {
    std::unique_lock<std::mutex> lk(mtx_);
    if (timeoutMs == 0) {
        if (lines_.empty()) return std::nullopt;
    } else if (timeoutMs < 0) {
        cv_.wait(lk, [this] { return !lines_.empty() || stopping_; });
    } else {
        cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                     [this] { return !lines_.empty() || stopping_; });
    }
    if (lines_.empty()) return std::nullopt;
    std::string s = std::move(lines_.front());
    lines_.pop_front();
    return s;
}

std::optional<std::string> ConsoleIoThread::tryRead() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (lines_.empty()) return std::nullopt;
    std::string s = std::move(lines_.front());
    lines_.pop_front();
    return s;
}

bool ConsoleIoThread::empty() {
    std::lock_guard<std::mutex> lk(mtx_);
    return lines_.empty();
}

void ConsoleIoThread::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (stopping_) return;
            lines_.push_back(line);
        }
        cv_.notify_all();
    }
    // EOF: push a sentinel "exit" so the main loop can terminate
    // gracefully if it wants.
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
}

} // namespace nb
