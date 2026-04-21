#include "nb/emulator_packet.hpp"

#include <gtest/gtest.h>

using namespace nb;

TEST(EmulatorPacketTest, RoundTrip) {
    std::vector<std::uint8_t> payload{'h', 'i'};
    EmulatorPacket p(5, 9, payload);

    auto packed = p.pack();
    ASSERT_EQ(packed.size(), static_cast<std::size_t>(EmulatorPacket::HEADER_SIZE + payload.size()));
    EXPECT_EQ(packed[0], 5);
    EXPECT_EQ(packed[1], 9);
    EXPECT_EQ(packed[2], static_cast<std::uint8_t>(EmulatorPacket::HEADER_SIZE + payload.size()));

    auto unpacked = EmulatorPacket::unpack(packed);
    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(unpacked->getDest(), 5);
    EXPECT_EQ(unpacked->getSrc(), 9);
    EXPECT_EQ(unpacked->getPayload(), payload);
}

TEST(EmulatorPacketTest, HeaderSize) {
    EXPECT_EQ(EmulatorPacket::HEADER_SIZE, 3);
    EXPECT_EQ(EmulatorPacket::MAX_PACKET_SIZE, 131);
}
