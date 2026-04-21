#include "nb/tcp_socket.hpp"

#include "nb/tcp_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace nb {

TcpSocket::TcpSocket(TcpManager* tcpMan)
    : state_(State::CLOSED),
      localPort_(-1),
      remoteAddr_(-1),
      remotePort_(-1),
      backlog_(0),
      tcpMan_(tcpMan) {}

int TcpSocket::bind(int localPort) {
    localPort_ = localPort;
    return 0;
}

int TcpSocket::listen(int backlog) {
    if (state_ != State::CLOSED) return -1;
    state_ = State::LISTEN;
    backlog_ = backlog;
    tcpMan_->registerListener(this, localPort_, backlog);
    return 0;
}

TcpSocket* TcpSocket::accept() {
    if (state_ != State::LISTEN) return nullptr;
    if (pendingConnQueue_.empty()) return nullptr;
    TcpSocket* conn = pendingConnQueue_.front();
    pendingConnQueue_.pop_front();
    if (conn->state_ != State::CLOSED) {
        conn->state_ = State::ESTABLISHED;
    }
    return conn;
}

int TcpSocket::connect(int destAddr, int destPort) {
    if (localPort_ < 0 || destPort < 0 || destPort > Transport::MAX_PORT_NUM) {
        return -1;
    }
    remoteAddr_ = destAddr;
    remotePort_ = destPort;
    synSeq_ = sndNxt_;
    if (!tcpMan_->sendSyn(this, destAddr, destPort)) return -1;
    tcpMan_->registerSynPending(this, destAddr, destPort);
    state_ = State::SYN_SENT;
    return 0;
}

void TcpSocket::close() {
    if (state_ == State::CLOSED) return;
    if (state_ == State::LISTEN || state_ == State::SYN_SENT) {
        state_ = State::CLOSED;
        return;
    }
    if (state_ == State::ESTABLISHED) {
        state_ = State::SHUTDOWN;
        maybeSendFinOnDrain();
        return;
    }
}

void TcpSocket::release() {
    if (state_ == State::CLOSED) return;
    onRelease();
    pendingConnQueue_.clear();
    backlog_ = 0;
    remoteAddr_ = -1;
    remotePort_ = -1;
    localPort_ = -1;
    state_ = State::CLOSED;
}

int TcpSocket::write(const std::uint8_t* buf, int pos, int len) {
    if (state_ != State::ESTABLISHED || !buf || pos < 0 || len < 0) return -1;
    if (len == 0) return 0;

    bool wasIdle = !hasOutstanding();
    int totalWritten = 0;

    while (len > 0) {
        int flight = sndNxt_ - sndUna_;
        int eff = effectiveWindow();
        if (flight >= eff) break;
        int space = eff - flight;
        int chunk = std::min(len,
                             std::min(space, Transport::MAX_PAYLOAD_SIZE));
        if (chunk <= 0) break;

        std::vector<std::uint8_t> payload(buf + pos, buf + pos + chunk);
        int seqStart = sndNxt_;
        if (!tcpMan_->sendData(this, remoteAddr_, remotePort_, seqStart,
                                payload, false)) {
            return -1;
        }

        std::int64_t now = tcpMan_->managerNowMs();
        OutstandingSeg seg;
        seg.seqStart = seqStart;
        seg.len = chunk;
        seg.data = std::move(payload);
        seg.firstSentAtMs = now;
        seg.retransmitted = false;
        pushOutstanding(std::move(seg));

        sndNxt_ += chunk;
        pos += chunk;
        totalWritten += chunk;
        len -= chunk;
    }

    if (wasIdle && totalWritten > 0) {
        std::int64_t now = tcpMan_->managerNowMs();
        nextRtoDeadlineMs_ = now + rtoMs_;
    }
    return totalWritten;
}

int TcpSocket::read(std::uint8_t* buf, int pos, int len) {
    if ((state_ != State::ESTABLISHED && state_ != State::SHUTDOWN) ||
        !buf || pos < 0 || len < 0) {
        return -1;
    }
    if (len == 0 || recvBufUsed_ == 0) {
        if (peerFinReceived_ && recvBufUsed_ == 0) state_ = State::CLOSED;
        return 0;
    }
    int n = std::min(len, recvBufUsed_);
    for (int i = 0; i < n; ++i) {
        buf[pos + i] = recvBuf_.front();
        recvBuf_.pop_front();
    }
    recvBufUsed_ -= n;
    if (peerFinReceived_ && recvBufUsed_ == 0) state_ = State::CLOSED;
    return n;
}

void TcpSocket::initAccepted(int localPort, int remoteAddr, int remotePort) {
    localPort_ = localPort;
    remoteAddr_ = remoteAddr;
    remotePort_ = remotePort;
    sndUna_ = 1;
    sndNxt_ = 1;
    rcvNxt_ = 1;
    cwnd_ = INITIAL_CWND_BYTES;
    state_ = State::ESTABLISHED;
}

void TcpSocket::enqueueAccepted(TcpSocket* child) {
    if (static_cast<int>(pendingConnQueue_.size()) < backlog_) {
        pendingConnQueue_.push_back(child);
    }
}

void TcpSocket::markEstablished() {
    if (state_ != State::CLOSED) {
        sndUna_ = 1;
        sndNxt_ = 1;
        rcvNxt_ = 1;
        cwnd_ = INITIAL_CWND_BYTES;
        peerFinReceived_ = false;
        state_ = State::ESTABLISHED;
    }
}

void TcpSocket::markClosed() { state_ = State::CLOSED; }

void TcpSocket::markPeerFinReceived() {
    peerFinReceived_ = true;
    if (state_ == State::ESTABLISHED) state_ = State::SHUTDOWN;
}

void TcpSocket::onRelease() { tcpMan_->unregisterSocket(this); }

void TcpSocket::maybeSendFinOnDrain() {
    if (state_ != State::SHUTDOWN) return;
    if (finSeq_ >= 0) return;
    if (hasOutstanding()) return;
    finSeq_ = sndNxt_;
    if (!tcpMan_->sendFin(this, remoteAddr_, remotePort_)) release();
}

int TcpSocket::appendToRecvBuf(const std::uint8_t* payload, int pos, int len) {
    if (!payload || len <= 0) return 0;
    int space = RECV_BUF_CAP - recvBufUsed_;
    int n = std::min(len, space);
    for (int i = 0; i < n; ++i) recvBuf_.push_back(payload[pos + i]);
    recvBufUsed_ += n;
    return n;
}

void TcpSocket::additiveIncreaseCwnd() {
    int base = std::max(cwnd_, CWND_MIN_BYTES);
    int inc = std::max(1, (Transport::MAX_PAYLOAD_SIZE *
                           Transport::MAX_PAYLOAD_SIZE) /
                              base);
    cwnd_ += inc;
}

void TcpSocket::multiplicativeDecreaseCwnd() {
    cwnd_ = std::max(
        CWND_MIN_BYTES,
        static_cast<int>(std::floor(cwnd_ * AIMD_MD_FACTOR)));
}

std::optional<TcpSocket::OutstandingSeg> TcpSocket::popAckedThrough(int ackNum) {
    std::optional<OutstandingSeg> lastPopped;
    while (!outstanding_.empty()) {
        const auto& head = outstanding_.front();
        if (head.seqStart + head.len <= ackNum) {
            lastPopped = std::move(outstanding_.front());
            outstanding_.pop_front();
        } else {
            break;
        }
    }
    sndUna_ = ackNum;
    if (outstanding_.empty()) {
        nextRtoDeadlineMs_ = 0;
    } else {
        nextRtoDeadlineMs_ = tcpMan_->managerNowMs() + rtoMs_;
    }
    return lastPopped;
}

void TcpSocket::onSampleRTT(std::int64_t sampleMs) {
    if (!hasEstRTT_) {
        estRTTms_ = static_cast<double>(sampleMs);
        devRTTms_ = sampleMs / 2.0;
        hasEstRTT_ = true;
    } else {
        double sample = static_cast<double>(sampleMs);
        double oldEstRTT = estRTTms_;
        estRTTms_ = (1.0 - ALPHA) * estRTTms_ + ALPHA * sample;
        devRTTms_ = (1.0 - BETA) * devRTTms_ +
                    BETA * std::fabs(sample - oldEstRTT);
    }
}

void TcpSocket::doubleRto() {
    rtoMs_ = std::min<long>(rtoMs_ * 2, RTO_MAX_MS);
}

} // namespace nb
