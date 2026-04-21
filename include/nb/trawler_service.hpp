#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace nb {

class EmulatedNode;
class TrawlerCommandsParser;

// TrawlerService: the central coordinator server that assigns node ids,
// keeps the topology, and ships add/remove-neighbor notifications to each
// emulated node. Mirrors Java Trawler.java.
class TrawlerService {
public:
    TrawlerService(int listenPort, const std::string& topoFile, bool allToAll);
    ~TrawlerService();

    TrawlerService(const TrawlerService&) = delete;
    TrawlerService& operator=(const TrawlerService&) = delete;

    // Accept loop. Never returns under normal operation.
    void run();

    // Topology hooks used by TrawlerCommandsParser.
    void startEdge(int a, int b);
    void failEdge(int a, int b);
    void failNode(int a);
    void restartNode(int a);
    void exit();

    // Called when an emulated node's TCP socket goes away.
    void remove(EmulatedNode* dyingNode);

private:
    void checkNodesQuit();
    void updateNeighbors(int fishAddr);
    void removeAsNeighbor(EmulatedNode* dyingNode);
    int freeFishAddr();
    bool portConflict(const std::string& ipAddress, int udpPort);
    EmulatedNode* getEmulatedNode(int fishAddr);

    int listenPort_ = 0;
    std::string topoFile_;
    bool allToAll_ = false;
    bool running_ = false;
    int listenFd_ = -1;

    std::unordered_map<int, std::unique_ptr<EmulatedNode>> emulatedNodes_;
    std::unique_ptr<TrawlerCommandsParser> parser_;
};

} // namespace nb
