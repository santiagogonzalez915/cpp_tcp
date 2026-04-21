#pragma once

#include "nb/packet.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nb {

class Transport {
public:
    static constexpr int MAX_PACKET_SIZE = Packet::MAX_PAYLOAD_SIZE;
    static constexpr int HEADER_SIZE = 12;
    static constexpr int MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;
    static constexpr int MAX_PORT_NUM = 255;

    static constexpr int SYN = 0;
    static constexpr int ACK = 1;
    static constexpr int FIN = 2;
    static constexpr int DATA = 3;

    Transport(int srcPort, int destPort, int type, int window, int seqNum,
              std::vector<std::uint8_t> payload);

    int getSrcPort() const { return srcPort_; }
    int getDestPort() const { return destPort_; }
    int getType() const { return type_; }
    int getWindow() const { return window_; }
    int getSeqNum() const { return seqNum_; }
    const std::vector<std::uint8_t>& getPayload() const { return payload_; }

    std::vector<std::uint8_t> pack() const;
    static std::optional<Transport> unpack(const std::vector<std::uint8_t>& data);
    static std::optional<Transport> unpack(const std::uint8_t* data, std::size_t len);

private:
    int srcPort_;
    int destPort_;
    int type_;
    int window_;
    int seqNum_;
    std::vector<std::uint8_t> payload_;
};

} // namespace nb
