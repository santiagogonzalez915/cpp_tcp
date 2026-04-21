#pragma once

#include "nb/packet.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nb {

// LinkState: variable-length list of 1-byte neighbor addresses carried in
// the payload of a Packet (protocol LINK_INFO_PKT). Kept for parity with Java
// library API; not dispatched by the default StackNode.
class LinkState {
public:
    static constexpr int MAX_NEIGHBORS = Packet::MAX_PAYLOAD_SIZE;

    explicit LinkState(const std::vector<int>& neighbors);

    std::vector<int> getNeighbors() const;
    std::vector<std::uint8_t> pack() const { return neighbors_; }

    static std::optional<LinkState> unpack(const std::vector<std::uint8_t>& data);

private:
    explicit LinkState(std::vector<std::uint8_t> packed)
        : neighbors_(std::move(packed)) {}

    std::vector<std::uint8_t> neighbors_;
};

} // namespace nb
