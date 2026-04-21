#include "nb/simulation_commands_parser.hpp"

#include "nb/simulator.hpp"

#include <iostream>

namespace nb {

void SimulationCommandsParser::parseNodeCmd(
    const std::vector<std::string>& cmd) {
    if (cmd.size() < 2) return;
    try {
        int nodeAddr = std::stoi(cmd[0]);
        std::string msg;
        for (std::size_t i = 1; i < cmd.size(); ++i) {
            msg += cmd[i];
            msg += ' ';
        }
        if (!msg.empty()) msg.pop_back();

        if (!simulator_->sendNodeMsg(nodeAddr, msg)) {
            std::cerr << "Node address: " << nodeAddr << " does not exist!"
                      << std::endl;
        }
    } catch (...) {
        std::cerr << "Error parsing command to node: ";
        printStrArray(cmd, std::cerr);
    }
}

void SimulationCommandsParser::exitCmd(const std::vector<std::string>& cmd) {
    if (cmd[0] == "exit") {
        simulator_->stop();
    }
}

} // namespace nb
