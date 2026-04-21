#pragma once

#include "nb/commands_parser.hpp"

namespace nb {

class Emulator;

class EmulationCommandsParser : public CommandsParser {
public:
    explicit EmulationCommandsParser(Emulator* emulator)
        : emulator_(emulator) {}

    // Emulator parser does NOT go through edge/fail/restart branches — it
    // always dispatches to common/node commands.
    std::int64_t parseLine(const std::string& line,
                           std::int64_t nowMicros) override;

protected:
    void parseNodeCmd(const std::vector<std::string>& cmd) override;
    void exitCmd(const std::vector<std::string>& cmd) override;

private:
    Emulator* emulator_;
};

} // namespace nb
