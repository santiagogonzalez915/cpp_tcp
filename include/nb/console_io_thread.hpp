#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace nb {

// ConsoleIoThread: background thread that reads lines from stdin and hands
// them to the event loop. Mirrors Java IOThread.java.
class ConsoleIoThread {
public:
    ConsoleIoThread();
    ~ConsoleIoThread();

    ConsoleIoThread(const ConsoleIoThread&) = delete;
    ConsoleIoThread& operator=(const ConsoleIoThread&) = delete;

    void start();
    void stop();

    // Block up to `timeoutMs` milliseconds for a line. Returns empty optional
    // if no line was available within that window (timeoutMs < 0 means block
    // indefinitely, timeoutMs == 0 means don't block).
    std::optional<std::string> waitForLine(long timeoutMs);

    // Non-blocking: pop next line if any.
    std::optional<std::string> tryRead();

    bool empty();

private:
    void run();

    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::string> lines_;
    bool stopping_ = false;
    bool started_ = false;
};

} // namespace nb
