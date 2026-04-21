#include "nb/runtime_manager.hpp"

#include "nb/commands_parser.hpp"
#include "nb/packet.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace nb {

RuntimeManager::RuntimeManager(std::int64_t startMicros)
    : startMs_(startMicros / 1000) {}

void RuntimeManager::stop() {
    std::cerr << "NetBridge exiting after time: "
              << (now() - startMs_) << " msec."
              << "\nNumber of packets sent: " << pktsSent_ << std::endl;
    std::cerr << "Number of packets dropped: " << pktsDropped_ << std::endl;
    std::cerr << "Number of packets lost: " << pktsLost_ << std::endl;
    std::exit(0);
}

int RuntimeManager::setCommandFile(const std::string& filename) {
    auto f = std::make_unique<std::ifstream>(filename);
    if (!f->is_open()) {
        return -1;
    }
    commandFile_ = std::move(f);
    return 0;
}

bool RuntimeManager::sendPkt(int from, int to,
                             const std::vector<std::uint8_t>& pkt) {
    if (static_cast<int>(pkt.size()) > Packet::MAX_PACKET_SIZE ||
        !Packet::validAddress(to) || !Packet::validAddress(from)) {
        throw std::invalid_argument(
            "Invalid pkt or address in sendPkt");
    }
    auto unpacked = Packet::unpack(pkt);
    if (!unpacked.has_value() || !unpacked->isValidToSend()) {
        throw std::invalid_argument(
            "Invalid pkt content or TTL in sendPkt");
    }
    ++pktsSent_;
    return true;
}

void RuntimeManager::addTimerAt(int /*nodeAddr*/, std::int64_t tMs,
                                TimerCallback cb) {
    if (!cb) return;
    // Java stores times in microseconds in the queue.
    sortedEvents_.addEvent(ScheduledEvent(tMs * 1000, std::move(cb)));
}

void RuntimeManager::addTimer(int nodeAddr, std::int64_t deltaMs,
                              TimerCallback cb) {
    addTimerAt(nodeAddr, now() + deltaMs, std::move(cb));
}

void RuntimeManager::setParser(std::unique_ptr<CommandsParser> parser) {
    parser_ = std::move(parser);
}

std::int64_t RuntimeManager::readCommandFile(std::int64_t deferTill) {
    if (commandFile_ && deferTill <= (now() * 1000)) {
        std::string line;
        if (std::getline(*commandFile_, line)) {
            std::int64_t r = parser_->parseLine(line, now() * 1000);
            return std::max<std::int64_t>(r, 0);
        }
        commandFile_.reset();
    }
    if (!commandFile_) {
        return -1;
    }
    return deferTill;
}

void RuntimeManager::addEventAtMicros(std::int64_t timeMicros,
                                      TimerCallback cb) {
    if (timeMicros < 0 || !cb) return;
    sortedEvents_.addEvent(ScheduledEvent(timeMicros, std::move(cb)));
}

} // namespace nb
