#pragma once

#include "nb/timer_task.hpp"

#include <cstdint>
#include <vector>

namespace nb {

class TcpSocket;

// TransferClient: TCP sender for the transfer application. Mirrors Java
// TransferClient.java.
class TransferClient : public TimerTask {
public:
    static constexpr long DEFAULT_CLIENT_INTERVAL = 1000;
    static constexpr int DEFAULT_BUFFER_SZ = 65536;

    TransferClient(RuntimeManager* manager, StackNode* node, TcpSocket* sock,
                   int amount, long intervalMs = DEFAULT_CLIENT_INTERVAL,
                   int bufSize = DEFAULT_BUFFER_SZ);

    void execute() override;

private:
    TcpSocket* sock_;
    std::vector<std::uint8_t> buf_;
    int amount_;
    std::int64_t startTime_ = 0;
    std::int64_t finishTime_ = 0;
    int pos_ = 0;
};

} // namespace nb
