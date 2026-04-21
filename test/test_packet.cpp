#include "nb/packet.hpp"
#include "nb/protocol.hpp"
#include "nb/string_bytes.hpp"

#include <gtest/gtest.h>

using namespace nb;

TEST(PacketTest, RoundTrip) {
    std::vector<std::uint8_t> payload = stringToBytes("hello, world!");
    Packet p(3, 7, 12, Protocol::PING_PKT, 0x12345678, payload);

    auto packed = p.pack();
    ASSERT_EQ(packed.size(), static_cast<std::size_t>(Packet::HEADER_SIZE + payload.size()));
    EXPECT_EQ(packed[0], 3);
    EXPECT_EQ(packed[1], 7);
    EXPECT_EQ(packed[2], 12);
    EXPECT_EQ(packed[3], Protocol::PING_PKT);
    EXPECT_EQ(packed[4], static_cast<std::uint8_t>(payload.size() + Packet::HEADER_SIZE));
    EXPECT_EQ(packed[5], 0x12);
    EXPECT_EQ(packed[6], 0x34);
    EXPECT_EQ(packed[7], 0x56);
    EXPECT_EQ(packed[8], 0x78);

    auto unpacked = Packet::unpack(packed);
    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(unpacked->getDest(), 3);
    EXPECT_EQ(unpacked->getSrc(), 7);
    EXPECT_EQ(unpacked->getTTL(), 12);
    EXPECT_EQ(unpacked->getProtocol(), Protocol::PING_PKT);
    EXPECT_EQ(unpacked->getSeq(), 0x12345678);
    EXPECT_EQ(unpacked->getPayload(), payload);
}

TEST(PacketTest, SeqZeroRoundTrips) {
    Packet p(0, 1, 15, Protocol::TRANSPORT_PKT, 0, {});
    auto packed = p.pack();
    ASSERT_EQ(packed.size(), static_cast<std::size_t>(Packet::HEADER_SIZE));
    EXPECT_EQ(packed[5], 0);
    EXPECT_EQ(packed[6], 0);
    EXPECT_EQ(packed[7], 0);
    EXPECT_EQ(packed[8], 0);

    auto unpacked = Packet::unpack(packed);
    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(unpacked->getSeq(), 0);
}

TEST(PacketTest, FrozenConstants) {
    EXPECT_EQ(Packet::HEADER_SIZE, 9);
    EXPECT_EQ(Packet::MAX_PACKET_SIZE, 128);
    EXPECT_EQ(Packet::MAX_PAYLOAD_SIZE, 119);
    EXPECT_EQ(Packet::MAX_ADDRESS, 255);
    EXPECT_EQ(Packet::BROADCAST_ADDRESS, 255);
    EXPECT_EQ(Packet::MAX_TTL, 15);
}

TEST(PacketTest, RejectsTooLargePayload) {
    std::vector<std::uint8_t> payload(Packet::MAX_PAYLOAD_SIZE + 1, 0);
    EXPECT_THROW(Packet(1, 2, 3, Protocol::PING_PKT, 0, payload),
                 std::invalid_argument);
}

TEST(PacketTest, ValidToSendRequiresNonZeroTtl) {
    Packet p(1, 2, 0, Protocol::PING_PKT, 0, {});
    EXPECT_TRUE(p.isValid());
    EXPECT_FALSE(p.isValidToSend());
}
