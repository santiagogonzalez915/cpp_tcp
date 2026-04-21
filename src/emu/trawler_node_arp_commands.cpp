#include "nb/trawler_node_arp_commands.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace nb {

namespace {
std::vector<std::string> splitSpace(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            out.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(std::move(cur));
    while (!out.empty() && out.back().empty()) {
        out.pop_back();
    }
    if (out.empty()) out.push_back("");
    return out;
}

// Format a double the way Java's Double.toString / String.valueOf(double)
// does: the shortest decimal representation that round-trips back to the
// same double. Java guarantees at least one digit after the decimal point
// (0.0 -> "0.0", 0.5 -> "0.5", 0.05 -> "0.05").
std::string javaDouble(double d) {
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d < 0 ? "-Infinity" : "Infinity";

    // Try increasing precision until the value round-trips. With %g we
    // also need to strip trailing zeros while preserving one digit after
    // the dot.
    auto format = [](double v, int prec) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        return std::string(buf);
    };
    std::string s;
    for (int prec = 1; prec <= 17; ++prec) {
        s = format(d, prec);
        double back = 0.0;
        try {
            back = std::stod(s);
        } catch (...) {
            continue;
        }
        if (back == d) break;
    }

    // Ensure there is a '.' if no exponent present.
    bool hasDot = s.find('.') != std::string::npos;
    bool hasExp = s.find('e') != std::string::npos ||
                  s.find('E') != std::string::npos;
    if (!hasDot && !hasExp) s += ".0";
    return s;
}
} // namespace

std::string TrawlerNodeARPCommands::addNeighbor(int nodeId,
                                                const std::string& ipAddress,
                                                int port) {
    std::ostringstream oss;
    oss << "add " << nodeId << ' ' << ipAddress << ' ' << port;
    return oss.str();
}

std::string TrawlerNodeARPCommands::addNeighborOptions(
    int nodeId, const std::string& ipAddress, int port,
    const EdgeOptions& options) {
    std::ostringstream oss;
    oss << addNeighbor(nodeId, ipAddress, port);
    oss << " lossRate " << javaDouble(options.getLossRate());
    oss << " delay " << options.getDelay();
    oss << " bw " << options.getBW();
    oss << " bt " << options.getBT();
    return oss.str();
}

std::string TrawlerNodeARPCommands::removeNeighbor(int nodeId) {
    std::ostringstream oss;
    oss << "remove " << nodeId;
    return oss.str();
}

std::string TrawlerNodeARPCommands::reset() { return "reset"; }

std::optional<AddNeighborResult>
TrawlerNodeARPCommands::receiveAddNeighborOptions(const std::string& cmd) {
    auto args = splitSpace(cmd);
    if (args.size() != 12 || args[0] != "add" || args[4] != "lossRate" ||
        args[6] != "delay" || args[8] != "bw" || args[10] != "bt") {
        return std::nullopt;
    }
    try {
        AddNeighborResult r;
        r.nodeId = std::stoi(args[1]);
        r.ipAddress = args[2];
        r.port = std::stoi(args[3]);
        EdgeOptions opts;
        opts.setLossRate(std::stod(args[5]));
        opts.setDelay(std::stoll(args[7]));
        opts.setBW(std::stoi(args[9]));
        opts.setBT(std::stoll(args[11]));
        r.options = opts;
        return r;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<AddNeighborResult>
TrawlerNodeARPCommands::receiveAddNeighbor(const std::string& cmd) {
    auto withOpts = receiveAddNeighborOptions(cmd);
    if (withOpts) return withOpts;

    auto args = splitSpace(cmd);
    if (args.size() != 4 || args[0] != "add") return std::nullopt;
    try {
        AddNeighborResult r;
        r.nodeId = std::stoi(args[1]);
        r.ipAddress = args[2];
        r.port = std::stoi(args[3]);
        return r;
    } catch (...) {
        return std::nullopt;
    }
}

int TrawlerNodeARPCommands::receiveRemoveNeighbor(const std::string& cmd) {
    auto args = splitSpace(cmd);
    if (args.size() != 2 || args[0] != "remove") return -1;
    try {
        return std::stoi(args[1]);
    } catch (...) {
        return -1;
    }
}

bool TrawlerNodeARPCommands::receiveReset(const std::string& cmd) {
    return cmd == "reset";
}

} // namespace nb
