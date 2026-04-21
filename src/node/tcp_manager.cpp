#include "nb/tcp_manager.hpp"

#include "nb/packet.hpp"
#include "nb/protocol.hpp"
#include "nb/runtime_manager.hpp"
#include "nb/stack_node.hpp"
#include "nb/tcp_socket.hpp"
#include "nb/transport.hpp"

#include <iostream>

namespace nb {

TcpManager::TcpManager(StackNode* node, int addr, RuntimeManager* manager)
    : node_(node), addr_(addr), manager_(manager) {}

TcpManager::~TcpManager() = default;

void TcpManager::start() {
    manager_->addTimer(addr_, RETRANS_TICK_MS,
                       [this] { retransmissionTick(); });
}

void TcpManager::retransmissionTick() {
    std::int64_t now = managerNowMs();
    for (auto& kv : establishedByKey_) {
        TcpSocket* sock = kv.second;
        if (!sock || !sock->hasOutstanding()) continue;
        std::int64_t deadline = sock->getNextRtoDeadlineMs();
        if (deadline == 0 || now < deadline) continue;

        sock->timeoutCount++;
        sock->doubleRto();
        sock->multiplicativeDecreaseCwnd();

        for (auto& seg : sock->getOutstandingSegments()) {
            sendData(sock, sock->getRemoteAddr(), sock->getRemotePort(),
                     seg.seqStart, seg.data, true);
            seg.retransmitted = true;
        }
        sock->setNextRtoDeadlineMs(now + sock->getCurrentRtoMs());
    }
    manager_->addTimer(addr_, RETRANS_TICK_MS,
                       [this] { retransmissionTick(); });
}

TcpSocket* TcpManager::socket() {
    auto sock = std::make_unique<TcpSocket>(this);
    TcpSocket* raw = sock.get();
    allSockets_.push_back(std::move(sock));
    return raw;
}

void TcpManager::receiveTransport(int srcAddr, const Transport& transport) {
    switch (transport.getType()) {
    case Transport::SYN:
        std::cout << "S" << std::flush;
        onSyn(srcAddr, transport);
        break;
    case Transport::ACK:
        onAck(srcAddr, transport);
        break;
    case Transport::FIN:
        std::cout << "F" << std::flush;
        onFin(srcAddr, transport);
        break;
    case Transport::DATA:
        onData(srcAddr, transport);
        break;
    default:
        break;
    }
}

void TcpManager::onSyn(int srcAddr, const Transport& syn) {
    int localPort = syn.getDestPort();
    int remotePort = syn.getSrcPort();

    auto listenerIt = listenersByPort_.find(localPort);
    if (listenerIt == listenersByPort_.end()) {
        sendFinFromPorts(localPort, srcAddr, remotePort);
        return;
    }
    TcpSocket* listener = listenerIt->second;

    TcpSocket* child = socket();
    child->initAccepted(localPort, srcAddr, remotePort);
    listener->enqueueAccepted(child);
    registerEstablished(child, srcAddr, remotePort);

    int ackNum = syn.getSeqNum() + 1;
    sendAck(localPort, srcAddr, remotePort, ackNum, true,
            child->getRecvBufAvailable());
}

void TcpManager::onAck(int srcAddr, const Transport& ack) {
    ConnKey key{srcAddr, ack.getSrcPort(), ack.getDestPort()};

    auto pendingIt = synPendingByKey_.find(key);
    if (pendingIt != synPendingByKey_.end()) {
        TcpSocket* pending = pendingIt->second;
        int expectedAck = pending->getSynSeq() + 1;
        if (ack.getSeqNum() == expectedAck) {
            pending->setRwnd(ack.getWindow());
            pending->markEstablished();
            synPendingByKey_.erase(pendingIt);
            registerEstablished(pending, srcAddr, ack.getSrcPort());
            std::cout << ":" << std::flush;
        } else {
            std::cout << "?" << std::flush;
        }
        return;
    }

    auto estIt = establishedByKey_.find(key);
    TcpSocket* established = (estIt != establishedByKey_.end()) ? estIt->second : nullptr;

    if (established && established->isClosurePending() &&
        established->getFinSeq() >= 0) {
        int expectedAck = established->getFinSeq() + 1;
        if (ack.getSeqNum() == expectedAck) {
            established->markClosed();
            establishedByKey_.erase(estIt);
            std::cout << ":" << std::flush;
            return;
        }
    }

    if (established &&
        (established->isConnected() || established->isClosurePending())) {
        int ackNum = ack.getSeqNum();
        int sndUna = established->getSndUna();
        int sndNxt = established->getSndNxt();

        if (ackNum > sndNxt) {
            std::cout << "?" << std::flush;
            return;
        }
        established->setRwnd(ack.getWindow());
        if (ackNum <= sndUna) {
            std::cout << "?" << std::flush;
            return;
        }

        auto popped = established->popAckedThrough(ackNum);
        established->additiveIncreaseCwnd();
        if (popped && !popped->retransmitted) {
            std::int64_t sampleRTT = managerNowMs() - popped->firstSentAtMs;
            established->onSampleRTT(sampleRTT);
        }
        established->maybeSendFinOnDrain();
        std::cout << ":" << std::flush;
        return;
    }
    std::cout << "?" << std::flush;
}

void TcpManager::onFin(int srcAddr, const Transport& fin) {
    ConnKey key{srcAddr, fin.getSrcPort(), fin.getDestPort()};
    auto estIt = establishedByKey_.find(key);
    if (estIt != establishedByKey_.end()) {
        TcpSocket* established = estIt->second;
        established->markPeerFinReceived();
        sendAck(fin.getDestPort(), srcAddr, fin.getSrcPort(),
                fin.getSeqNum() + 1, true,
                established->getRecvBufAvailable());
        return;
    }
    auto synIt = synPendingByKey_.find(key);
    if (synIt != synPendingByKey_.end()) {
        TcpSocket* pending = synIt->second;
        synPendingByKey_.erase(synIt);
        pending->markClosed();
    }
}

void TcpManager::onData(int srcAddr, const Transport& data) {
    ConnKey key{srcAddr, data.getSrcPort(), data.getDestPort()};
    auto estIt = establishedByKey_.find(key);
    if (estIt == establishedByKey_.end()) return;
    TcpSocket* established = estIt->second;
    if (!(established->isConnected() || established->isClosurePending()))
        return;

    int segSeq = data.getSeqNum();
    const auto& payload = data.getPayload();
    int segLen = static_cast<int>(payload.size());

    if (segLen == 0) {
        sendAck(established->getLocalPort(), srcAddr, data.getSrcPort(),
                established->getRcvNxt(), false,
                established->getRecvBufAvailable());
        return;
    }
    if (segSeq == established->getRcvNxt()) {
        int avail = established->getRecvBufAvailable();
        if (avail < segLen) {
            std::cout << "!" << std::flush;
            sendAck(established->getLocalPort(), srcAddr, data.getSrcPort(),
                    established->getRcvNxt(), false,
                    established->getRecvBufAvailable());
            return;
        }
        int accepted =
            established->appendToRecvBuf(payload.data(), 0, segLen);
        established->setRcvNxt(established->getRcvNxt() + accepted);
        std::cout << "." << std::flush;
        sendAck(established->getLocalPort(), srcAddr, data.getSrcPort(),
                established->getRcvNxt(), true,
                established->getRecvBufAvailable());
        return;
    }
    std::cout << "!" << std::flush;
    sendAck(established->getLocalPort(), srcAddr, data.getSrcPort(),
            established->getRcvNxt(), false,
            established->getRecvBufAvailable());
}

bool TcpManager::sendSyn(TcpSocket* sock, int destAddr, int destPort) {
    try {
        Transport syn(sock->getLocalPort(), destPort, Transport::SYN, 0,
                      sock->getSynSeq(), {});
        Packet pkt(destAddr, addr_, Packet::MAX_TTL, Protocol::TRANSPORT_PKT,
                   0, syn.pack());
        manager_->sendPkt(addr_, destAddr, pkt.pack());
        std::cout << "S" << std::flush;
        return true;
    } catch (const std::exception& e) {
        node_->logError(std::string("Failed to send SYN: ") + e.what());
        return false;
    }
}

bool TcpManager::sendFin(TcpSocket* sock, int destAddr, int destPort) {
    try {
        Transport fin(sock->getLocalPort(), destPort, Transport::FIN, 0,
                      sock->getFinSeq(), {});
        Packet pkt(destAddr, addr_, Packet::MAX_TTL, Protocol::TRANSPORT_PKT,
                   0, fin.pack());
        manager_->sendPkt(addr_, destAddr, pkt.pack());
        std::cout << "F" << std::flush;
        return true;
    } catch (const std::exception& e) {
        node_->logError(std::string("Failed to send FIN: ") + e.what());
        return false;
    }
}

bool TcpManager::sendData(TcpSocket* sock, int destAddr, int destPort,
                          int seqNum,
                          const std::vector<std::uint8_t>& payload,
                          bool retransmitted) {
    try {
        Transport data(sock->getLocalPort(), destPort, Transport::DATA, 0,
                       seqNum, payload);
        Packet pkt(destAddr, addr_, Packet::MAX_TTL, Protocol::TRANSPORT_PKT,
                   0, data.pack());
        manager_->sendPkt(addr_, destAddr, pkt.pack());
        std::cout << (retransmitted ? "!" : ".") << std::flush;
        return true;
    } catch (const std::exception& e) {
        node_->logError(std::string("Failed to send DATA: ") + e.what());
        return false;
    }
}

bool TcpManager::sendAck(int srcPort, int destAddr, int destPort, int ackNum,
                          bool advances, int window) {
    try {
        Transport ack(srcPort, destPort, Transport::ACK, window, ackNum, {});
        Packet pkt(destAddr, addr_, Packet::MAX_TTL, Protocol::TRANSPORT_PKT,
                   0, ack.pack());
        manager_->sendPkt(addr_, destAddr, pkt.pack());
        std::cout << (advances ? ":" : "?") << std::flush;
        return true;
    } catch (const std::exception& e) {
        node_->logError(std::string("Failed to send ACK: ") + e.what());
        return false;
    }
}

bool TcpManager::sendFinFromPorts(int srcPort, int destAddr, int destPort) {
    try {
        Transport fin(srcPort, destPort, Transport::FIN, 0, 0, {});
        Packet pkt(destAddr, addr_, Packet::MAX_TTL, Protocol::TRANSPORT_PKT,
                   0, fin.pack());
        manager_->sendPkt(addr_, destAddr, pkt.pack());
        std::cout << "F" << std::flush;
        return true;
    } catch (const std::exception& e) {
        node_->logError(std::string("Failed to send FIN: ") + e.what());
        return false;
    }
}

void TcpManager::registerListener(TcpSocket* sock, int localPort,
                                   int /*backlog*/) {
    listenersByPort_[localPort] = sock;
}

void TcpManager::registerSynPending(TcpSocket* sock, int remoteAddr,
                                     int remotePort) {
    ConnKey k{remoteAddr, remotePort, sock->getLocalPort()};
    synPendingByKey_[k] = sock;
}

void TcpManager::registerEstablished(TcpSocket* sock, int remoteAddr,
                                      int remotePort) {
    ConnKey k{remoteAddr, remotePort, sock->getLocalPort()};
    establishedByKey_[k] = sock;
}

void TcpManager::unregisterSocket(TcpSocket* sock) {
    for (auto it = listenersByPort_.begin(); it != listenersByPort_.end();) {
        if (it->second == sock) it = listenersByPort_.erase(it); else ++it;
    }
    for (auto it = synPendingByKey_.begin(); it != synPendingByKey_.end();) {
        if (it->second == sock) it = synPendingByKey_.erase(it); else ++it;
    }
    for (auto it = establishedByKey_.begin(); it != establishedByKey_.end();) {
        if (it->second == sock) it = establishedByKey_.erase(it); else ++it;
    }
}

std::int64_t TcpManager::managerNowMs() const { return manager_->now(); }

} // namespace nb
