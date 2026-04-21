#pragma once

#include "nb/edge_options.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nb {

struct AddNeighborResult {
    int nodeId = -1;
    std::string ipAddress;  // numeric dotted-quad or hostname as sent
    int port = 0;
    std::optional<EdgeOptions> options;  // populated for the "Options" form
};

// TrawlerNodeARPCommands: one-line text commands between Trawler and an
// emulated node. Mirrors Java TrawlerNodeARPCommands. Byte-for-byte wire
// compatibility is required so a Java Trawler ↔ C++ emulator can talk.
class TrawlerNodeARPCommands {
public:
    static std::string addNeighbor(int nodeId, const std::string& ipAddress,
                                   int port);
    static std::string addNeighborOptions(int nodeId,
                                          const std::string& ipAddress,
                                          int port,
                                          const EdgeOptions& options);
    static std::string removeNeighbor(int nodeId);
    static std::string reset();

    static std::optional<AddNeighborResult> receiveAddNeighbor(
        const std::string& cmd);
    // Returns -1 if not a remove-neighbor command.
    static int receiveRemoveNeighbor(const std::string& cmd);
    static bool receiveReset(const std::string& cmd);

private:
    static std::optional<AddNeighborResult> receiveAddNeighborOptions(
        const std::string& cmd);
};

} // namespace nb
