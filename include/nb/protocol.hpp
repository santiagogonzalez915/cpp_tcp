#pragma once

#include <string>

namespace nb {

// Protocol numbers carried in Packet.protocol. Values match Java Protocol.java.
struct Protocol {
    static constexpr int PING_PKT = 0;
    static constexpr int PING_REPLY_PKT = 1;
    static constexpr int LINK_INFO_PKT = 2;
    static constexpr int NAME_PKT = 3;
    static constexpr int TRANSPORT_PKT = 4;

    static bool isValid(int protocol) {
        return protocol == PING_PKT || protocol == PING_REPLY_PKT ||
               protocol == LINK_INFO_PKT || protocol == NAME_PKT ||
               protocol == TRANSPORT_PKT;
    }

    static std::string toString(int protocol) {
        switch (protocol) {
            case PING_PKT:       return "Ping Packet";
            case PING_REPLY_PKT: return "Ping Reply Packet";
            case LINK_INFO_PKT:  return "Link State Packet";
            case NAME_PKT:       return "Name Packet";
            case TRANSPORT_PKT:  return "Transport Packet";
            default:             return "Unknown Protocol";
        }
    }
};

} // namespace nb
