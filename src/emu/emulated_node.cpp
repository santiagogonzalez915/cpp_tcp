#include "nb/emulated_node.hpp"

#include "nb/edge.hpp"
#include "nb/topology.hpp"
#include "nb/trawler_node_arp_commands.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace nb {

EmulatedNode::EmulatedNode(int tcpFd, int fishAddr,
                           const std::string& ipAddress, int udpPort)
    : tcpFd_(tcpFd), fishAddr_(fishAddr), ipAddress_(ipAddress),
      udpPort_(udpPort) {}

EmulatedNode::~EmulatedNode() { close(); }

bool EmulatedNode::isAlive() const { return alive_ && tcpFd_ >= 0; }

void EmulatedNode::close() {
    if (tcpFd_ >= 0) {
        ::close(tcpFd_);
        tcpFd_ = -1;
    }
    alive_ = false;
}

bool EmulatedNode::writeLine(const std::string& line) {
    if (tcpFd_ < 0) return false;
    std::string withNl = line + '\n';
    std::size_t off = 0;
    while (off < withNl.size()) {
        ssize_t n = ::send(tcpFd_, withNl.data() + off, withNl.size() - off,
                           0);
        if (n <= 0) {
            alive_ = false;
            return false;
        }
        off += static_cast<std::size_t>(n);
    }
    return true;
}

bool EmulatedNode::sendAddNeighbor(const EmulatedNode& peer) {
    Edge* e = Topology::instance().getLiveEdge(fishAddr_, peer.fishAddr_);
    std::string cmd;
    if (e) {
        cmd = TrawlerNodeARPCommands::addNeighborOptions(
            peer.fishAddr_, peer.ipAddress_, peer.udpPort_, e->getOptions());
    } else {
        cmd = TrawlerNodeARPCommands::addNeighbor(
            peer.fishAddr_, peer.ipAddress_, peer.udpPort_);
    }
    return writeLine(cmd);
}

bool EmulatedNode::sendRemoveNeighbor(int peerFishAddr) {
    return writeLine(TrawlerNodeARPCommands::removeNeighbor(peerFishAddr));
}

bool EmulatedNode::sendReset() {
    return writeLine(TrawlerNodeARPCommands::reset());
}

} // namespace nb
