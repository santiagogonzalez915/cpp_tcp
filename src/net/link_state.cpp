#include "nb/link_state.hpp"

#include <stdexcept>

namespace nb {

LinkState::LinkState(const std::vector<int>& neighbors) {
    if (static_cast<int>(neighbors.size()) > MAX_NEIGHBORS) {
        throw std::invalid_argument(
            "Number of neighbors is greater than max allowed");
    }
    neighbors_.reserve(neighbors.size());
    for (int n : neighbors) {
        neighbors_.push_back(static_cast<std::uint8_t>(n));
    }
}

std::vector<int> LinkState::getNeighbors() const {
    std::vector<int> out;
    out.reserve(neighbors_.size());
    for (std::uint8_t b : neighbors_) {
        out.push_back(b);
    }
    return out;
}

std::optional<LinkState> LinkState::unpack(
    const std::vector<std::uint8_t>& data) {
    if (static_cast<int>(data.size()) > MAX_NEIGHBORS) {
        return std::nullopt;
    }
    return LinkState(std::vector<std::uint8_t>(data));
}

} // namespace nb
