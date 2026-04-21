#pragma once

#include "nb/packet.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nb {

class EmulatorPacket {
public:
    static constexpr int HEADER_SIZE = 3;
    static constexpr int MAX_PACKET_SIZE = Packet::MAX_PACKET_SIZE + HEADER_SIZE;

    EmulatorPacket(int destAddr, int srcAddr, std::vector<std::uint8_t> payload);

    int getDest() const { return destAddr_; }
    int getSrc() const { return srcAddr_; }
    const std::vector<std::uint8_t>& getPayload() const { return payload_; }

    std::vector<std::uint8_t> pack() const;
    static std::optional<EmulatorPacket> unpack(
        const std::vector<std::uint8_t>& data);
    static std::optional<EmulatorPacket> unpack(
        const std::uint8_t* data, std::size_t len);

private:
    int destAddr_;
    int srcAddr_;
    std::vector<std::uint8_t> payload_;
};

} // namespace nb
