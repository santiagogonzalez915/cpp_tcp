#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nb {

inline std::vector<std::uint8_t> stringToBytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

inline std::string bytesToString(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

inline std::string bytesToString(const std::uint8_t* data, std::size_t len) {
    return std::string(reinterpret_cast<const char*>(data), len);
}

} // namespace nb
