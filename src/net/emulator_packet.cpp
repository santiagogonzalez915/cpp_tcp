#include "nb/emulator_packet.hpp"

#include <stdexcept>

namespace nb {

EmulatorPacket::EmulatorPacket(int destAddr, int srcAddr,
                               std::vector<std::uint8_t> payload)
    : destAddr_(destAddr), srcAddr_(srcAddr), payload_(std::move(payload)) {
    if (HEADER_SIZE + static_cast<int>(payload_.size()) > MAX_PACKET_SIZE) {
        throw std::invalid_argument("Payload is too big");
    }
}

std::vector<std::uint8_t> EmulatorPacket::pack() const {
    std::vector<std::uint8_t> out;
    out.reserve(HEADER_SIZE + payload_.size());
    out.push_back(static_cast<std::uint8_t>(destAddr_));
    out.push_back(static_cast<std::uint8_t>(srcAddr_));
    out.push_back(static_cast<std::uint8_t>(HEADER_SIZE + payload_.size()));
    out.insert(out.end(), payload_.begin(), payload_.end());
    return out;
}

std::optional<EmulatorPacket> EmulatorPacket::unpack(
    const std::vector<std::uint8_t>& data) {
    return unpack(data.data(), data.size());
}

std::optional<EmulatorPacket> EmulatorPacket::unpack(const std::uint8_t* data,
                                                     std::size_t len) {
    if (len < static_cast<std::size_t>(HEADER_SIZE)) {
        return std::nullopt;
    }
    int destAddr = data[0];
    int srcAddr = data[1];
    int packetLength = data[2];
    int payloadLen = packetLength - HEADER_SIZE;
    if (payloadLen < 0 ||
        static_cast<std::size_t>(HEADER_SIZE + payloadLen) > len) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> payload(data + HEADER_SIZE,
                                      data + HEADER_SIZE + payloadLen);
    try {
        return EmulatorPacket(destAddr, srcAddr, std::move(payload));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

} // namespace nb
