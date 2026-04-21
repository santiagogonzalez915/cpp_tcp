#include "nb/emulation_commands_parser.hpp"

#include "nb/emulator.hpp"

namespace nb {

std::int64_t EmulationCommandsParser::parseLine(const std::string& line,
                                                std::int64_t nowMicros) {
    if (skipLine(line)) return -1;
    auto cmd = splitSpaces(line);
    return parseCommonCmds(cmd, nowMicros);
}

void EmulationCommandsParser::parseNodeCmd(
    const std::vector<std::string>& cmd) {
    std::string msg;
    for (const auto& s : cmd) {
        msg += s;
        msg += ' ';
    }
    if (!msg.empty()) msg.pop_back();
    // Node address does not matter for the emulator (single local node).
    emulator_->sendNodeMsg(0, msg);
}

void EmulationCommandsParser::exitCmd(const std::vector<std::string>& cmd) {
    if (cmd[0] == "exit") {
        emulator_->stop();
    }
}

} // namespace nb
