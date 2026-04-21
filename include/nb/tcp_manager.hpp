#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace nb {

class StackNode;
class RuntimeManager;
class TcpSocket;
class Transport;

// TcpManager: demultiplexes transport packets to the right TcpSocket,
// issues retransmission ticks, and exposes per-socket send helpers.
// Mirrors Java TCPManager.java.
class TcpManager {
public:
    static constexpr long RETRANS_TICK_MS = 10;

    TcpManager(StackNode* node, int addr, RuntimeManager* manager);
    ~TcpManager();

    void start();
    void retransmissionTick();

    TcpSocket* socket();

    // Incoming dispatch.
    void receiveTransport(int srcAddr, const Transport& transport);

    // Outgoing helpers used by TcpSocket.
    bool sendSyn(TcpSocket* sock, int destAddr, int destPort);
    bool sendFin(TcpSocket* sock, int destAddr, int destPort);
    bool sendData(TcpSocket* sock, int destAddr, int destPort, int seqNum,
                  const std::vector<std::uint8_t>& payload,
                  bool retransmitted);

    // Registration hooks.
    void registerListener(TcpSocket* sock, int localPort, int backlog);
    void registerSynPending(TcpSocket* sock, int remoteAddr, int remotePort);
    void registerEstablished(TcpSocket* sock, int remoteAddr, int remotePort);
    void unregisterSocket(TcpSocket* sock);

    std::int64_t managerNowMs() const;

private:
    struct ConnKey {
        int remoteAddr;
        int remotePort;
        int localPort;
        bool operator==(const ConnKey& o) const {
            return remoteAddr == o.remoteAddr &&
                   remotePort == o.remotePort && localPort == o.localPort;
        }
    };
    struct ConnKeyHash {
        std::size_t operator()(const ConnKey& k) const noexcept {
            std::size_t h = 17;
            h = 31 * h + static_cast<std::size_t>(k.remoteAddr);
            h = 31 * h + static_cast<std::size_t>(k.remotePort);
            h = 31 * h + static_cast<std::size_t>(k.localPort);
            return h;
        }
    };

    void onSyn(int srcAddr, const Transport& syn);
    void onAck(int srcAddr, const Transport& ack);
    void onFin(int srcAddr, const Transport& fin);
    void onData(int srcAddr, const Transport& data);
    bool sendAck(int srcPort, int destAddr, int destPort, int ackNum,
                 bool advances, int window);
    bool sendFinFromPorts(int srcPort, int destAddr, int destPort);

    StackNode* node_;
    int addr_;
    RuntimeManager* manager_;

    std::vector<std::unique_ptr<TcpSocket>> allSockets_;
    std::unordered_map<int, TcpSocket*> listenersByPort_;
    std::unordered_map<ConnKey, TcpSocket*, ConnKeyHash> synPendingByKey_;
    std::unordered_map<ConnKey, TcpSocket*, ConnKeyHash> establishedByKey_;
};

} // namespace nb
