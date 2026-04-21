#pragma once

#include "nb/transport.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <list>
#include <optional>
#include <vector>

namespace nb {

class TcpManager;

// TcpSocket: mirrors Java TCPSock.java. State machine, send/recv buffers,
// RTT/RTO, AIMD congestion control. Semantic parity is the goal; wire
// format is handled by Transport.
class TcpSocket {
public:
    static constexpr int RECV_BUF_CAP = 32 * 1024;
    static constexpr int SEND_BUF_CAP = 32 * 1024;

    static constexpr double ALPHA = 0.125;
    static constexpr double BETA = 0.25;
    static constexpr long RTO_INITIAL_MS = 1000;
    static constexpr long RTO_MIN_MS = 1;
    static constexpr long RTO_MAX_MS = 64000L;

    static constexpr int INITIAL_CWND_BYTES = Transport::MAX_PAYLOAD_SIZE;
    static constexpr int CWND_MIN_BYTES = Transport::MAX_PAYLOAD_SIZE;
    static constexpr double AIMD_MD_FACTOR = 0.5;

    enum class State { CLOSED, LISTEN, SYN_SENT, ESTABLISHED, SHUTDOWN };

    struct OutstandingSeg {
        int seqStart = 0;
        int len = 0;
        std::vector<std::uint8_t> data;
        std::int64_t firstSentAtMs = 0;
        bool retransmitted = false;
    };

    explicit TcpSocket(TcpManager* tcpMan);

    // --- App-level connection setup ---------------------------------------
    int bind(int localPort);
    int listen(int backlog);
    TcpSocket* accept();
    int connect(int destAddr, int destPort);
    void close();
    void release();

    // --- Data transfer ---------------------------------------------------
    int write(const std::uint8_t* buf, int pos, int len);
    int read(std::uint8_t* buf, int pos, int len);

    // --- State predicates ------------------------------------------------
    bool isConnectionPending() const { return state_ == State::SYN_SENT; }
    bool isClosed() const { return state_ == State::CLOSED; }
    bool isConnected() const { return state_ == State::ESTABLISHED; }
    bool isClosurePending() const { return state_ == State::SHUTDOWN; }

    // --- Accessors -------------------------------------------------------
    int getLocalPort() const { return localPort_; }
    int getRemoteAddr() const { return remoteAddr_; }
    int getRemotePort() const { return remotePort_; }

    // --- TcpManager hooks (connection lifecycle) -------------------------
    void initAccepted(int localPort, int remoteAddr, int remotePort);
    void enqueueAccepted(TcpSocket* child);
    void markEstablished();
    void markClosed();
    void markPeerFinReceived();
    void onRelease();
    void maybeSendFinOnDrain();

    // --- Receive buffer --------------------------------------------------
    int appendToRecvBuf(const std::uint8_t* payload, int pos, int len);
    int getRecvBufAvailable() const { return RECV_BUF_CAP - recvBufUsed_; }
    int getRcvNxt() const { return rcvNxt_; }
    void setRcvNxt(int v) { rcvNxt_ = v; }

    // --- Sender sequence -------------------------------------------------
    int getSynSeq() const { return synSeq_; }
    int getFinSeq() const { return finSeq_; }
    int getSndUna() const { return sndUna_; }
    void setSndUna(int v) { sndUna_ = v; }
    int getSndNxt() const { return sndNxt_; }
    int getBytesInFlight() const { return sndNxt_ - sndUna_; }

    bool senderWindowHasCapacity() const {
        return getBytesInFlight() < effectiveWindow();
    }
    int effectiveWindow() const {
        return std::min(rwnd_, cwnd_);
    }
    int getCwnd() const { return cwnd_; }
    int getRwnd() const { return rwnd_; }
    void setRwnd(int v) { rwnd_ = std::max(0, v); }

    void additiveIncreaseCwnd();
    void multiplicativeDecreaseCwnd();

    // --- Outstanding segments -------------------------------------------
    bool hasOutstanding() const { return !outstanding_.empty(); }
    void pushOutstanding(OutstandingSeg seg) {
        outstanding_.push_back(std::move(seg));
    }
    // Pop all fully-ACKed segments; returns the last popped (empty if none).
    // Updates sndUna_ and nextRtoDeadlineMs_.
    std::optional<OutstandingSeg> popAckedThrough(int ackNum);
    std::deque<OutstandingSeg>& getOutstandingSegments() {
        return outstanding_;
    }

    // --- RTT / RTO -------------------------------------------------------
    long getCurrentRtoMs() const { return rtoMs_; }
    long getRtoMs() const { return rtoMs_; }
    void onSampleRTT(std::int64_t sampleMs);
    void doubleRto();
    std::int64_t getNextRtoDeadlineMs() const { return nextRtoDeadlineMs_; }
    void setNextRtoDeadlineMs(std::int64_t d) { nextRtoDeadlineMs_ = d; }

    // Timeout counter, visible to TcpManager.
    int timeoutCount = 0;

private:
    State state_;
    int localPort_;
    int remoteAddr_;
    int remotePort_;

    int backlog_;
    std::list<TcpSocket*> pendingConnQueue_;

    int sndUna_ = 0;
    int sndNxt_ = 0;
    int rcvNxt_ = 0;

    std::deque<std::uint8_t> recvBuf_;
    int recvBufUsed_ = 0;

    int synSeq_ = -1;
    int finSeq_ = -1;

    double estRTTms_ = RTO_INITIAL_MS;
    double devRTTms_ = 0;
    bool hasEstRTT_ = false;
    bool peerFinReceived_ = false;
    std::deque<OutstandingSeg> outstanding_;
    std::int64_t nextRtoDeadlineMs_ = 0;
    long rtoMs_ = RTO_INITIAL_MS;

    int rwnd_ = RECV_BUF_CAP;
    int cwnd_ = INITIAL_CWND_BYTES;

    TcpManager* tcpMan_;
};

} // namespace nb
