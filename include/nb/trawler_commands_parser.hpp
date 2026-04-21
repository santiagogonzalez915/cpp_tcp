#pragma once

#include "nb/commands_parser.hpp"

namespace nb {

class TrawlerService;

// TrawlerCommandsParser: topology-only parser. Forwards to Topology (via
// the base class) and also notifies the Trawler service.
class TrawlerCommandsParser : public CommandsParser {
public:
    explicit TrawlerCommandsParser(TrawlerService* trawler)
        : trawler_(trawler) {}

protected:
    void createNewEdge(int a, int b, const EdgeOptions& opts) override;
    bool failEdgeHook(int a, int b) override;
    void failNodeHook(int a) override;
    bool restartEdgeHook(int a, int b) override;
    void restartNodeHook(int a) override;

    void parseNodeCmd(const std::vector<std::string>& cmd) override;
    void exitCmd(const std::vector<std::string>& cmd) override;

private:
    TrawlerService* trawler_;
};

} // namespace nb
