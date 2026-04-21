#pragma once

#include "nb/commands_parser.hpp"

namespace nb {

class Simulator;

class SimulationCommandsParser : public CommandsParser {
public:
    explicit SimulationCommandsParser(Simulator* simulator)
        : simulator_(simulator) {}

protected:
    void parseNodeCmd(const std::vector<std::string>& cmd) override;
    void exitCmd(const std::vector<std::string>& cmd) override;

private:
    Simulator* simulator_;
};

} // namespace nb
