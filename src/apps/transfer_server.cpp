#include "nb/transfer_server.hpp"

#include "nb/runtime_manager.hpp"
#include "nb/stack_node.hpp"
#include "nb/tcp_socket.hpp"

#include <memory>

namespace nb {

TransferServer::TransferServer(RuntimeManager* manager, StackNode* node,
                               TcpSocket* serverSock, long serverInterval,
                               long workerInterval, int bufSize)
    : TimerTask(manager, node, serverInterval),
      serverSock_(serverSock),
      workerInterval_(workerInterval),
      bufSize_(bufSize) {
    setInterval(serverInterval);
}

void TransferServer::execute() {
    if (!serverSock_->isClosed()) {
        TcpSocket* conn = serverSock_->accept();
        if (!conn) return;
        node_->logOutput("time = " + std::to_string(manager_->now()) + " msec");
        node_->logOutput("connection accepted");
        auto worker = std::make_shared<TransferWorker>(
            manager_, node_, conn, workerInterval_, bufSize_);
        worker->start();
    } else {
        node_->logOutput("time = " + std::to_string(manager_->now()) + " msec");
        node_->logOutput("server shutdown");
        stop();
    }
}

TransferWorker::TransferWorker(RuntimeManager* manager, StackNode* node,
                               TcpSocket* sock, long intervalMs, int bufSize)
    : TimerTask(manager, node, intervalMs), sock_(sock), buf_(bufSize) {
    setInterval(intervalMs);
}

void TransferWorker::execute() {
    if (!sock_->isClosed()) {
        int index = pos_ % static_cast<int>(buf_.size());
        int len = static_cast<int>(buf_.size()) - index;
        int count = sock_->read(buf_.data(), index, len);
        if (count == -1) {
            node_->logError("time = " + std::to_string(manager_->now()) +
                            " msec");
            node_->logError("receiving aborted");
            node_->logError("position = " + std::to_string(pos_));
            node_->logError("releasing connection...");
            sock_->release();
            stop();
            return;
        }
        if (count > 0) {
            for (int i = index; i < index + count; ++i) {
                if (buf_[i] != static_cast<std::uint8_t>(i)) {
                    node_->logError("time = " + std::to_string(manager_->now()) +
                                    " msec");
                    node_->logError("data corruption detected");
                    node_->logError("position = " + std::to_string(pos_));
                    node_->logError("releasing connection...");
                    sock_->release();
                    stop();
                    return;
                }
            }
        }
        pos_ += count;
        return;
    }
    node_->logOutput("time = " + std::to_string(manager_->now()) + " msec");
    node_->logOutput("connection closed");
    node_->logOutput("total bytes received = " + std::to_string(pos_));
    stop();
}

} // namespace nb
