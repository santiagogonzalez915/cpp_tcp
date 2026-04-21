#include "nb/trawler_node_arp_commands.hpp"

#include <gtest/gtest.h>

using namespace nb;

TEST(TrawlerArpCommandsTest, AddNeighborBasic) {
    std::string s = TrawlerNodeARPCommands::addNeighbor(3, "127.0.0.1", 9000);
    EXPECT_EQ(s, "add 3 127.0.0.1 9000");
    auto r = TrawlerNodeARPCommands::receiveAddNeighbor(s);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->nodeId, 3);
    EXPECT_EQ(r->ipAddress, "127.0.0.1");
    EXPECT_EQ(r->port, 9000);
    EXPECT_FALSE(r->options.has_value());
}

TEST(TrawlerArpCommandsTest, AddNeighborWithOptions) {
    EdgeOptions opts;
    opts.setLossRate(0.05);
    opts.setDelay(200);
    opts.setBW(10000);
    opts.setBT(1000);
    std::string s = TrawlerNodeARPCommands::addNeighborOptions(
        5, "host.local", 12345, opts);
    EXPECT_EQ(s,
              "add 5 host.local 12345 lossRate 0.05 delay 200 bw 10000 bt 1000");
    auto r = TrawlerNodeARPCommands::receiveAddNeighbor(s);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->nodeId, 5);
    EXPECT_EQ(r->port, 12345);
    ASSERT_TRUE(r->options.has_value());
    EXPECT_DOUBLE_EQ(r->options->getLossRate(), 0.05);
    EXPECT_EQ(r->options->getDelay(), 200);
    EXPECT_EQ(r->options->getBW(), 10000);
    EXPECT_EQ(r->options->getBT(), 1000);
}

TEST(TrawlerArpCommandsTest, Remove) {
    EXPECT_EQ(TrawlerNodeARPCommands::removeNeighbor(7), "remove 7");
    EXPECT_EQ(TrawlerNodeARPCommands::receiveRemoveNeighbor("remove 7"), 7);
    EXPECT_EQ(TrawlerNodeARPCommands::receiveRemoveNeighbor("not a command"),
              -1);
}

TEST(TrawlerArpCommandsTest, Reset) {
    EXPECT_EQ(TrawlerNodeARPCommands::reset(), "reset");
    EXPECT_TRUE(TrawlerNodeARPCommands::receiveReset("reset"));
    EXPECT_FALSE(TrawlerNodeARPCommands::receiveReset("reset extra"));
}

TEST(TrawlerArpCommandsTest, JavaDoubleFormatZero) {
    EdgeOptions opts;
    opts.setLossRate(0.0);
    std::string s = TrawlerNodeARPCommands::addNeighborOptions(
        0, "x", 1, opts);
    // Java String.valueOf(0.0) -> "0.0"
    EXPECT_NE(s.find("lossRate 0.0 "), std::string::npos);
}
