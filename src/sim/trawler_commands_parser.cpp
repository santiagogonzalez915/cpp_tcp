#include "nb/trawler_commands_parser.hpp"

#include "nb/trawler_service.hpp"

#include <iostream>

namespace nb {

void TrawlerCommandsParser::createNewEdge(int a, int b,
                                          const EdgeOptions& opts) {
    CommandsParser::createNewEdge(a, b, opts);
    trawler_->startEdge(a, b);
}

bool TrawlerCommandsParser::failEdgeHook(int a, int b) {
    if (CommandsParser::failEdgeHook(a, b)) {
        trawler_->failEdge(a, b);
        return true;
    }
    return false;
}

void TrawlerCommandsParser::failNodeHook(int a) {
    trawler_->failNode(a);
    CommandsParser::failNodeHook(a);
}

bool TrawlerCommandsParser::restartEdgeHook(int a, int b) {
    if (CommandsParser::restartEdgeHook(a, b)) {
        trawler_->startEdge(a, b);
        return true;
    }
    return false;
}

void TrawlerCommandsParser::restartNodeHook(int a) {
    CommandsParser::restartNodeHook(a);
    trawler_->restartNode(a);
}

void TrawlerCommandsParser::parseNodeCmd(
    const std::vector<std::string>& cmd) {
    std::cerr << "Trawler: Could not understand command: ";
    printStrArray(cmd, std::cerr);
}

void TrawlerCommandsParser::exitCmd(const std::vector<std::string>& cmd) {
    if (cmd[0] == "exit") {
        trawler_->exit();
    }
}

} // namespace nb
