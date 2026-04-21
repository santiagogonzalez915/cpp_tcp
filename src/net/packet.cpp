#include "nb/packet.hpp"

#include "nb/protocol.hpp"
#include "nb/string_bytes.hpp"

#include <sstream>

namespace nb {

Packet::Packet(int dest, int src, int ttl, int protocol, int seq,
               std::vector<std::uint8_t> payload)
    : dest_(dest), src_(src), ttl_(ttl), protocol_(protocol), seq_(seq),
      payload_(std::move(payload)) {
    if (!isValid(dest_, src_, ttl_, protocol_,
                 static_cast<int>(payload_.size()) + HEADER_SIZE)) {
        throw std::invalid_argument(
            "Arguments passed to constructor of Packet are invalid");
    }
}

bool Packet::isValid() const {
    return isValid(dest_, src_, ttl_, protocol_,
                   static_cast<int>(payload_.size()) + HEADER_SIZE);
}

bool Packet::isValid(int dest, int src, int ttl, int protocol, int size) {
    return dest <= MAX_ADDRESS && dest >= 0 &&
           validAddress(src) &&
           Protocol::isValid(protocol) &&
           ttl <= MAX_TTL && ttl >= 0 &&
           size <= MAX_PACKET_SIZE;
}

std::vector<std::uint8_t> Packet::pack() const {
    std::vector<std::uint8_t> out;
    out.reserve(HEADER_SIZE + payload_.size());
    out.push_back(static_cast<std::uint8_t>(dest_));
    out.push_back(static_cast<std::uint8_t>(src_));
    out.push_back(static_cast<std::uint8_t>(ttl_));
    out.push_back(static_cast<std::uint8_t>(protocol_));
    out.push_back(static_cast<std::uint8_t>(payload_.size() + HEADER_SIZE));

    // Java BigInteger.valueOf(seq).toByteArray() is big-endian signed
    // minimum-length representation; the code then left-pads with zeros to
    // 4 bytes. For a non-negative int fitting in 31 bits this is identical
    // to a 4-byte big-endian signed/unsigned representation.
    std::int32_t s = seq_;
    out.push_back(static_cast<std::uint8_t>((s >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((s >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((s >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(s & 0xFF));

    out.insert(out.end(), payload_.begin(), payload_.end());
    return out;
}

std::optional<Packet> Packet::unpack(const std::vector<std::uint8_t>& data) {
    return unpack(data.data(), data.size());
}

std::optional<Packet> Packet::unpack(const std::uint8_t* data, std::size_t len) {
    if (len < static_cast<std::size_t>(HEADER_SIZE)) {
        return std::nullopt;
    }
    int dest = data[0];
    int src = data[1];
    int ttl = data[2];
    int protocol = data[3];
    int packetLength = data[4];

    std::int32_t seq =
        (static_cast<std::int32_t>(data[5]) << 24) |
        (static_cast<std::int32_t>(data[6]) << 16) |
        (static_cast<std::int32_t>(data[7]) << 8) |
        (static_cast<std::int32_t>(data[8]));

    std::vector<std::uint8_t> payload(data + HEADER_SIZE, data + len);
    if (HEADER_SIZE + static_cast<int>(payload.size()) != packetLength) {
        return std::nullopt;
    }
    try {
        return Packet(dest, src, ttl, protocol, seq, std::move(payload));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

std::string Packet::toString() const {
    std::ostringstream oss;
    oss << "Packet: " << src_ << "->" << dest_
        << " protocol: " << protocol_
        << " TTL: " << ttl_
        << " seq: " << seq_
        << " contents: " << bytesToString(payload_);
    return oss.str();
}

} // namespace nb
