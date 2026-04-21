#include "nb/transport.hpp"

#include <stdexcept>

namespace nb {

Transport::Transport(int srcPort, int destPort, int type, int window,
                     int seqNum, std::vector<std::uint8_t> payload)
    : srcPort_(srcPort), destPort_(destPort), type_(type), window_(window),
      seqNum_(seqNum), payload_(std::move(payload)) {
    if (srcPort_ < 0 || srcPort_ > MAX_PORT_NUM ||
        destPort_ < 0 || destPort_ > MAX_PORT_NUM ||
        type_ < SYN || type_ > DATA ||
        static_cast<int>(payload_.size()) > MAX_PAYLOAD_SIZE) {
        throw std::invalid_argument(
            "Illegal arguments given to Transport packet");
    }
}

static void writeBigEndian32(std::vector<std::uint8_t>& out, std::int32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

std::vector<std::uint8_t> Transport::pack() const {
    std::vector<std::uint8_t> out;
    out.reserve(HEADER_SIZE + payload_.size());
    out.push_back(static_cast<std::uint8_t>(srcPort_));
    out.push_back(static_cast<std::uint8_t>(destPort_));
    out.push_back(static_cast<std::uint8_t>(type_));
    writeBigEndian32(out, window_);
    writeBigEndian32(out, seqNum_);
    out.push_back(static_cast<std::uint8_t>(HEADER_SIZE + payload_.size()));
    out.insert(out.end(), payload_.begin(), payload_.end());
    return out;
}

std::optional<Transport> Transport::unpack(
    const std::vector<std::uint8_t>& data) {
    return unpack(data.data(), data.size());
}

std::optional<Transport> Transport::unpack(const std::uint8_t* data,
                                           std::size_t len) {
    if (len < static_cast<std::size_t>(HEADER_SIZE)) {
        return std::nullopt;
    }
    int srcPort = data[0];
    int destPort = data[1];
    int type = data[2];
    std::int32_t window =
        (static_cast<std::int32_t>(data[3]) << 24) |
        (static_cast<std::int32_t>(data[4]) << 16) |
        (static_cast<std::int32_t>(data[5]) << 8) |
        (static_cast<std::int32_t>(data[6]));
    std::int32_t seqNum =
        (static_cast<std::int32_t>(data[7]) << 24) |
        (static_cast<std::int32_t>(data[8]) << 16) |
        (static_cast<std::int32_t>(data[9]) << 8) |
        (static_cast<std::int32_t>(data[10]));
    int packetLength = data[11];
    int payloadLen = packetLength - HEADER_SIZE;
    if (payloadLen < 0 || static_cast<std::size_t>(HEADER_SIZE + payloadLen) != len) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> payload(data + HEADER_SIZE,
                                      data + HEADER_SIZE + payloadLen);
    try {
        return Transport(srcPort, destPort, type, window, seqNum,
                         std::move(payload));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

} // namespace nb
