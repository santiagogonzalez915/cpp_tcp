#include "nb/commands_parser.hpp"
#include "nb/topology.hpp"

#include <gtest/gtest.h>

using namespace nb;

namespace {
class TestParser : public CommandsParser {
public:
    std::vector<std::vector<std::string>> nodeCmds;
    bool exitSeen = false;
protected:
    void exitCmd(const std::vector<std::string>& cmd) override {
        if (!cmd.empty() && cmd[0] == "exit") exitSeen = true;
    }
    void parseNodeCmd(const std::vector<std::string>& cmd) override {
        nodeCmds.push_back(cmd);
    }
};
} // namespace

class CommandsParserTest : public ::testing::Test {
protected:
    void SetUp() override { Topology::reset(); }
    void TearDown() override { Topology::reset(); }
};

TEST_F(CommandsParserTest, EchoAndComments) {
    TestParser p;
    EXPECT_EQ(p.parseLine("# a comment", 0), -1);
    EXPECT_EQ(p.parseLine("// another", 0), -1);
    EXPECT_EQ(p.parseLine("", 0), -1);
    EXPECT_EQ(p.parseLine("echo hello world", 0), -1);
    EXPECT_EQ(p.nodeCmds.size(), 0u);
}

TEST_F(CommandsParserTest, TimeCommand) {
    TestParser p;
    // time 5 -> 5 ms -> 5000 us
    EXPECT_EQ(p.parseLine("time 5", 0), 5000);
    // time + 10 at now=1000us -> 1000 + 10*1000 = 11000
    EXPECT_EQ(p.parseLine("time + 10", 1000), 11000);
}

TEST_F(CommandsParserTest, EdgeCreation) {
    TestParser p;
    EXPECT_EQ(p.parseLine("edge 0 1", 0), -1);
    Edge* e = Topology::instance().getLiveEdge(0, 1);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->getOptions().getLossRate(), 0.0);
    EXPECT_EQ(e->getOptions().getDelay(), 1);
}

TEST_F(CommandsParserTest, EdgeWithOptions) {
    TestParser p;
    EXPECT_EQ(p.parseLine("edge 0 1 lossRate 0.05 delay 200 bw 10000 bt 1000", 0), -1);
    Edge* e = Topology::instance().getLiveEdge(0, 1);
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(e->getOptions().getLossRate(), 0.05);
    EXPECT_EQ(e->getOptions().getDelay(), 200);
    EXPECT_EQ(e->getOptions().getBW(), 10000);
    EXPECT_EQ(e->getOptions().getBT(), 1000);
}

TEST_F(CommandsParserTest, NodeCommand) {
    TestParser p;
    p.parseLine("0 1 Hi there!", 100);
    ASSERT_EQ(p.nodeCmds.size(), 1u);
    EXPECT_EQ(p.nodeCmds[0][0], "0");
    EXPECT_EQ(p.nodeCmds[0][1], "1");
    EXPECT_EQ(p.nodeCmds[0][2], "Hi");
    EXPECT_EQ(p.nodeCmds[0][3], "there!");
}

TEST_F(CommandsParserTest, ExitRecognized) {
    TestParser p;
    p.parseLine("exit", 0);
    EXPECT_TRUE(p.exitSeen);
}

TEST_F(CommandsParserTest, FailRestartEdge) {
    TestParser p;
    p.parseLine("edge 0 1", 0);
    p.parseLine("fail 0 1", 0);
    EXPECT_EQ(Topology::instance().getLiveEdge(0, 1), nullptr);
    p.parseLine("restart 0 1", 0);
    EXPECT_NE(Topology::instance().getLiveEdge(0, 1), nullptr);
}
