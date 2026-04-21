#include "nb/commands_parser.hpp"

#include "nb/topology.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace nb {

std::vector<std::string> CommandsParser::splitSpaces(const std::string& line) {
    // Match Java's line.split(" ") exactly: consecutive spaces produce empty
    // tokens, leading space produces an empty first token, trailing spaces
    // produce empty trailing tokens that Java would then strip by the
    // split(-1 limit default). Actually Java split(" ") with default limit
    // trims trailing empty strings; we replicate that.
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : line) {
        if (c == ' ') {
            tokens.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    tokens.push_back(std::move(cur));
    while (!tokens.empty() && tokens.back().empty()) {
        tokens.pop_back();
    }
    // Java returns {""} (array of length 1 with empty string) if input is
    // empty. Match that.
    if (tokens.empty()) {
        tokens.push_back("");
    }
    return tokens;
}

bool CommandsParser::skipLine(const std::string& line) {
    if (line.empty()) return true;
    auto cmd = splitSpaces(line);
    if (cmd[0].rfind("//", 0) == 0 || cmd[0].rfind("#", 0) == 0) {
        return true;
    }
    return false;
}

std::int64_t CommandsParser::parseFile(const std::string& filename,
                                       std::int64_t nowMicros) {
    filename_ = filename;
    reader_ = std::make_unique<std::ifstream>(filename);
    if (!reader_->is_open()) {
        reader_.reset();
        throw std::runtime_error("Could not open topo/command file: " + filename);
    }
    return parseRemainder(nowMicros);
}

std::int64_t CommandsParser::parseRemainder(std::int64_t nowMicros) {
    if (!reader_) return -1;
    std::int64_t deferTill = -1;
    std::string line;
    while (deferTill == -1 && std::getline(*reader_, line)) {
        deferTill = parseLine(line, nowMicros);
    }
    return deferTill;
}

std::int64_t CommandsParser::parseLine(const std::string& line,
                                       std::int64_t nowMicros) {
    if (skipLine(line)) return -1;
    auto cmd = splitSpaces(line);

    if (parseEdge(cmd) || parseFail(cmd) || parseRestart(cmd)) {
        return -1;
    }
    return parseCommonCmds(cmd, nowMicros);
}

std::int64_t CommandsParser::parseCommonCmds(
    const std::vector<std::string>& cmd, std::int64_t nowMicros) {
    exitCmd(cmd);
    if (echo(cmd)) return -1;

    std::int64_t deferTill = parseTime(cmd, nowMicros);
    if (deferTill == -1 && cmd[0] != "time") {
        parseNodeCmd(cmd);
    }
    return deferTill;
}

void CommandsParser::createNewEdge(int a, int b, const EdgeOptions& opts) {
    Topology::instance().newEdge(a, b, opts);
}

bool CommandsParser::failEdgeHook(int a, int b) {
    return Topology::instance().failEdge(a, b);
}

void CommandsParser::failNodeHook(int a) {
    Topology::instance().failNode(a);
}

bool CommandsParser::restartEdgeHook(int a, int b) {
    return Topology::instance().restartEdge(a, b);
}

void CommandsParser::restartNodeHook(int a) {
    Topology::instance().restartNode(a);
}

void CommandsParser::printStrArray(const std::vector<std::string>& arr,
                                   std::ostream& os) {
    printStrArray(arr, 0, arr.size(), os);
}

void CommandsParser::printStrArray(const std::vector<std::string>& arr,
                                   std::size_t start, std::size_t end,
                                   std::ostream& os) {
    end = std::min(end, arr.size());
    for (std::size_t i = start; i < end; ++i) {
        os << arr[i] << ' ';
    }
    os << '\n';
}

bool CommandsParser::parseEdge(const std::vector<std::string>& cmd) {
    if (cmd[0] != "edge") return false;
    try {
        int nodeA = 0, nodeB = 0;
        EdgeOptions options;

        auto parseLong = [](const std::string& s) {
            return static_cast<std::int64_t>(std::stoll(s));
        };
        auto parseInt = [](const std::string& s) {
            return std::stoi(s);
        };
        auto parseDouble = [](const std::string& s) {
            return std::stod(s);
        };

        switch (cmd.size()) {
            case 11:
                if (cmd[9] == "bt") {
                    options.setBT(parseLong(cmd[10]));
                }
                [[fallthrough]];
            case 9:
                if (cmd[7] == "bw") {
                    options.setBW(parseInt(cmd[8]));
                }
                [[fallthrough]];
            case 7:
                if (cmd[5] == "delay") {
                    options.setDelay(parseLong(cmd[6]));
                }
                [[fallthrough]];
            case 5:
                if (cmd[3] == "lossRate") {
                    options.setLossRate(parseDouble(cmd[4]));
                }
                [[fallthrough]];
            case 3:
                nodeA = parseInt(cmd[1]);
                nodeB = parseInt(cmd[2]);
                break;
            default:
                throw std::runtime_error("bad edge cmd");
        }
        createNewEdge(nodeA, nodeB, options);
    } catch (...) {
        std::cerr << "Error parsing edge command: ";
        printStrArray(cmd, std::cerr);
    }
    return true;
}

bool CommandsParser::parseFail(const std::vector<std::string>& cmd) {
    if (cmd[0] != "fail") return false;
    try {
        int a = std::stoi(cmd.at(1));
        int b = -1;
        if (cmd.size() > 2) b = std::stoi(cmd[2]);
        if (b != -1) {
            if (!failEdgeHook(a, b)) {
                std::cerr << "No edge exists between node " << a
                          << " and node " << b << std::endl;
            }
        } else {
            failNodeHook(a);
        }
    } catch (...) {
        std::cerr << "Error parsing fail command: ";
        printStrArray(cmd, std::cerr);
    }
    return true;
}

bool CommandsParser::parseRestart(const std::vector<std::string>& cmd) {
    if (cmd[0] != "restart") return false;
    try {
        int a = std::stoi(cmd.at(1));
        int b = -1;
        if (cmd.size() > 2) b = std::stoi(cmd[2]);
        if (b != -1) {
            if (!restartEdgeHook(a, b)) {
                std::cerr << "No edge exists between node " << a
                          << " and node " << b << std::endl;
            }
        } else {
            restartNodeHook(a);
        }
    } catch (...) {
        std::cerr << "Error parsing restart command: ";
        printStrArray(cmd, std::cerr);
    }
    return true;
}

bool CommandsParser::echo(const std::vector<std::string>& cmd) {
    if (cmd[0] != "echo") return false;
    printStrArray(cmd, 1, cmd.size(), std::cout);
    return true;
}

std::int64_t CommandsParser::parseTime(const std::vector<std::string>& cmd,
                                       std::int64_t nowMicros) {
    if (cmd[0] != "time") return -1;
    try {
        if (cmd.at(1) == "+") {
            std::int64_t v = std::stoll(cmd.at(2));
            return nowMicros + (v * 1000);
        }
        std::int64_t v = std::stoll(cmd[1]);
        return v * 1000;
    } catch (...) {
        std::cerr << "Error parsing time command: ";
        printStrArray(cmd, std::cerr);
        return -1;
    }
}

} // namespace nb
