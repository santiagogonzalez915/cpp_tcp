#pragma once

#include "nb/edge_options.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace nb {

// CommandsParser: abstract base for the various command-script parsers.
// Mirrors Java CommandsParser. Commands are space-sensitive, line-oriented.
//
// Parse-deferral model: parseLine returns -1 to mean "no defer"; if a
// `time` command is hit, it returns the absolute microsecond time at
// which parsing should resume.
class CommandsParser {
public:
    virtual ~CommandsParser() = default;

    // Open `filename` and parse as much of it as possible. Returns the
    // defer-until time (microseconds) or -1 if no defer.
    std::int64_t parseFile(const std::string& filename, std::int64_t nowMicros);

    // Parse more of the already-open file. Returns the new defer-until
    // time.
    std::int64_t parseRemainder(std::int64_t nowMicros);

    // Parse one command line. Returns the defer-until time or -1.
    virtual std::int64_t parseLine(const std::string& line,
                                   std::int64_t nowMicros);

protected:
    virtual void exitCmd(const std::vector<std::string>& cmd) = 0;
    virtual void parseNodeCmd(const std::vector<std::string>& cmd) = 0;

    // Topology hooks: default implementations forward to Topology; the
    // Trawler parser overrides them to also notify the trawler service.
    virtual void createNewEdge(int a, int b, const EdgeOptions& opts);
    virtual bool failEdgeHook(int a, int b);
    virtual void failNodeHook(int a);
    virtual bool restartEdgeHook(int a, int b);
    virtual void restartNodeHook(int a);

    static std::vector<std::string> splitSpaces(const std::string& line);
    static bool skipLine(const std::string& line);

    // Parse common commands (echo/time/exit/node). Returns defer-until.
    std::int64_t parseCommonCmds(const std::vector<std::string>& cmd,
                                 std::int64_t nowMicros);

    static void printStrArray(const std::vector<std::string>& arr,
                              std::ostream& os);
    static void printStrArray(const std::vector<std::string>& arr,
                              std::size_t start, std::size_t end,
                              std::ostream& os);

private:
    bool parseEdge(const std::vector<std::string>& cmd);
    bool parseFail(const std::vector<std::string>& cmd);
    bool parseRestart(const std::vector<std::string>& cmd);
    bool echo(const std::vector<std::string>& cmd);
    std::int64_t parseTime(const std::vector<std::string>& cmd,
                           std::int64_t nowMicros);

    std::string filename_;
    std::unique_ptr<std::ifstream> reader_;
};

} // namespace nb
