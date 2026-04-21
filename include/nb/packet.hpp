#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb {

class Packet {
public:
    static constexpr int BROADCAST_ADDRESS = 255;
    static constexpr int MAX_ADDRESS = 255;
    static constexpr int HEADER_SIZE = 9;
    static constexpr int MAX_PACKET_SIZE = 128;
    static constexpr int MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;
    static constexpr int MAX_TTL = 15;

    Packet(int dest, int src, int ttl, int protocol, int seq,
           std::vector<std::uint8_t> payload);

    int getDest() const { return dest_; }
    int getSrc() const { return src_; }
    int getTTL() const { return ttl_; }
    void setTTL(int ttl) { ttl_ = ttl; }
    int getProtocol() const { return protocol_; }
    int getSeq() const { return seq_; }
    const std::vector<std::uint8_t>& getPayload() const { return payload_; }

    std::vector<std::uint8_t> pack() const;
    static std::optional<Packet> unpack(const std::vector<std::uint8_t>& data);
    static std::optional<Packet> unpack(const std::uint8_t* data, std::size_t len);

    static bool validAddress(int addr) {
        return addr <= MAX_ADDRESS && addr >= 0;
    }

    bool isValid() const;
    bool isValidToSend() const { return isValid() && ttl_ > 0; }

    std::string toString() const;

private:
    static bool isValid(int dest, int src, int ttl, int protocol, int size);

    int dest_;
    int src_;
    int ttl_;
    int protocol_;
    int seq_;
    std::vector<std::uint8_t> payload_;
};

} // namespace nb
