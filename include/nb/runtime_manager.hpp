#pragma once

#include "nb/commands_parser.hpp"
#include "nb/ordered_event_queue.hpp"
#include "nb/scheduled_event.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

namespace nb {

// RuntimeManager: abstract base for Simulator / Emulator.
// Mirrors Java Manager.java. Times in the public API are in milliseconds
// (like Java), but the internal queue stores absolute times in microseconds.
class RuntimeManager {
public:
    // Initialize with starting time in microseconds (emulator passes
    // clockMicros(), simulator passes 0).
    explicit RuntimeManager(std::int64_t startMicros);
    virtual ~RuntimeManager() = default;

    RuntimeManager(const RuntimeManager&) = delete;
    RuntimeManager& operator=(const RuntimeManager&) = delete;

    // Start the manager. Runs in an infinite loop until stop() is called.
    virtual void start() = 0;

    // Stop and exit, printing statistics to stderr.
    virtual void stop();

    // Set the command-script file (Java's setFishnetFile). Returns 0 on
    // success, -1 if the file cannot be opened.
    int setCommandFile(const std::string& filename);

    // Send a packet (byte-packed). Subclasses override, but they must call
    // the base to validate args and bump the packet counter.
    // Returns true on success.
    virtual bool sendPkt(int from, int to, const std::vector<std::uint8_t>& pkt);

    // Return current time in milliseconds.
    virtual std::int64_t now() const = 0;

    // Send a text msg to a node.
    virtual bool sendNodeMsg(int nodeAddr, const std::string& msg) = 0;

    // Real-time scaling (only valid for simulator). Base is a no-op.
    virtual void setTimescale(double /*timescale*/) {}

    // Schedule a callback to fire at wall-clock millisecond time `tMs`.
    // nodeAddr is used by some subclasses to validate the target node.
    virtual void addTimerAt(int nodeAddr, std::int64_t tMs, TimerCallback cb);

    // Schedule a callback to fire after deltaMs milliseconds from now().
    virtual void addTimer(int nodeAddr, std::int64_t deltaMs, TimerCallback cb);

    // Called by Edge when a packet is dropped (buffer overflow) or lost
    // (transmission error).
    void packetDropped() { ++pktsDropped_; }
    void packetLost() { ++pktsLost_; }

protected:
    void setParser(std::unique_ptr<CommandsParser> parser);
    CommandsParser* parser() { return parser_.get(); }

    // Read one line of the command-script file; returns the same deferTill
    // semantics as Java: -1 when EOF reached, 0 if no delay requested, or
    // the absolute microsecond time at which to resume parsing.
    std::int64_t readCommandFile(std::int64_t deferTill);

    // Schedule an event directly (for internal use).
    void addEventAtMicros(std::int64_t timeMicros, TimerCallback cb);

    OrderedEventQueue sortedEvents_;

private:
    int pktsSent_ = 0;
    int pktsDropped_ = 0;
    int pktsLost_ = 0;
    std::int64_t startMs_;
    std::unique_ptr<CommandsParser> parser_;
    std::unique_ptr<std::ifstream> commandFile_;
};

} // namespace nb
