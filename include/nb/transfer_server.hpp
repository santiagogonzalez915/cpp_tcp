#pragma once

#include "nb/timer_task.hpp"

#include <cstdint>
#include <vector>

namespace nb {

class TcpSocket;

// TransferServer: accepts TCP connections and spawns per-connection
// TransferWorker tasks that read and verify data. Mirrors Java
// TransferServer.java.
class TransferServer : public TimerTask {
public:
    static constexpr long DEFAULT_SERVER_INTERVAL = 1000;
    static constexpr long DEFAULT_WORKER_INTERVAL = 1000;
    static constexpr int DEFAULT_BUFFER_SZ = 65536;

    TransferServer(RuntimeManager* manager, StackNode* node,
                   TcpSocket* serverSock,
                   long serverInterval = DEFAULT_SERVER_INTERVAL,
                   long workerInterval = DEFAULT_WORKER_INTERVAL,
                   int bufSize = DEFAULT_BUFFER_SZ);

    void execute() override;

private:
    TcpSocket* serverSock_;
    long workerInterval_;
    int bufSize_;
};

// TransferWorker: TCP reader/verifier spawned by TransferServer.
class TransferWorker : public TimerTask {
public:
    TransferWorker(RuntimeManager* manager, StackNode* node, TcpSocket* sock,
                   long intervalMs, int bufSize);
    void execute() override;

private:
    TcpSocket* sock_;
    std::vector<std::uint8_t> buf_;
    int pos_ = 0;
};

} // namespace nb
