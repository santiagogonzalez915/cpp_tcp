#include "nb/transfer_client.hpp"

#include "nb/runtime_manager.hpp"
#include "nb/stack_node.hpp"
#include "nb/tcp_socket.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace nb {

TransferClient::TransferClient(RuntimeManager* manager, StackNode* node,
                               TcpSocket* sock, int amount, long intervalMs,
                               int bufSize)
    : TimerTask(manager, node, intervalMs),
      sock_(sock),
      buf_(bufSize),
      amount_(amount) {
    setInterval(intervalMs);
}

void TransferClient::execute() {
    if (sock_->isConnectionPending()) return;

    if (sock_->isConnected()) {
        if (startTime_ == 0) {
            startTime_ = manager_->now();
            node_->logOutput("time = " + std::to_string(startTime_) + " msec");
            node_->logOutput("started");
            node_->logOutput("bytes to send = " + std::to_string(amount_));
        }
        if (amount_ == 0) {
            node_->logOutput("time = " + std::to_string(manager_->now()));
            node_->logOutput("sending completed");
            node_->logOutput("closing connection...");
            sock_->close();
            return;
        }
        int index = pos_ % static_cast<int>(buf_.size());
        if (index == 0) {
            for (std::size_t i = 0; i < buf_.size(); ++i) {
                buf_[i] = static_cast<std::uint8_t>(i);
            }
        }
        int len = std::min(static_cast<int>(buf_.size()) - index, amount_);
        int count = sock_->write(buf_.data(), index, len);
        if (count == -1) {
            node_->logError("time = " + std::to_string(manager_->now()) +
                            " msec");
            node_->logError("sending aborted");
            node_->logError("position = " + std::to_string(pos_));
            node_->logError("releasing connection...");
            sock_->release();
            stop();
            return;
        }
        pos_ += count;
        amount_ -= count;
        return;
    }

    if (sock_->isClosurePending()) return;

    if (sock_->isClosed()) {
        finishTime_ = manager_->now();
        node_->logOutput("time = " + std::to_string(manager_->now()) + " msec");
        node_->logOutput("connection closed");
        node_->logOutput("total bytes sent = " + std::to_string(pos_));
        std::int64_t elapsed = finishTime_ - startTime_;
        node_->logOutput("time elapsed = " + std::to_string(elapsed) + " msec");
        if (elapsed > 0) {
            double bps = pos_ * 1000.0 / static_cast<double>(elapsed);
            node_->logOutput("Bps = " + std::to_string(bps));
        }
        sock_->release();
        stop();
        return;
    }
    node_->logError("TransferClient: unexpected socket state");
}

} // namespace nb
