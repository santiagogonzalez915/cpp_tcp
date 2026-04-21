#include "nb/transport.hpp"

#include <gtest/gtest.h>

using namespace nb;

TEST(TransportTest, RoundTrip) {
    std::vector<std::uint8_t> payload{'a', 'b', 'c', 'd'};
    Transport t(10, 21, Transport::DATA, 0x20304050, 0x01020304, payload);

    auto packed = t.pack();
    ASSERT_EQ(packed.size(), static_cast<std::size_t>(Transport::HEADER_SIZE + payload.size()));
    EXPECT_EQ(packed[0], 10);
    EXPECT_EQ(packed[1], 21);
    EXPECT_EQ(packed[2], Transport::DATA);
    EXPECT_EQ(packed[3], 0x20);
    EXPECT_EQ(packed[4], 0x30);
    EXPECT_EQ(packed[5], 0x40);
    EXPECT_EQ(packed[6], 0x50);
    EXPECT_EQ(packed[7], 0x01);
    EXPECT_EQ(packed[8], 0x02);
    EXPECT_EQ(packed[9], 0x03);
    EXPECT_EQ(packed[10], 0x04);
    EXPECT_EQ(packed[11], static_cast<std::uint8_t>(Transport::HEADER_SIZE + payload.size()));

    auto unpacked = Transport::unpack(packed);
    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(unpacked->getSrcPort(), 10);
    EXPECT_EQ(unpacked->getDestPort(), 21);
    EXPECT_EQ(unpacked->getType(), Transport::DATA);
    EXPECT_EQ(unpacked->getWindow(), 0x20304050);
    EXPECT_EQ(unpacked->getSeqNum(), 0x01020304);
    EXPECT_EQ(unpacked->getPayload(), payload);
}

TEST(TransportTest, HeaderSize) {
    EXPECT_EQ(Transport::HEADER_SIZE, 12);
}

TEST(TransportTest, EmptyPayload) {
    Transport t(1, 2, Transport::ACK, 0, 0, {});
    auto packed = t.pack();
    EXPECT_EQ(packed.size(), static_cast<std::size_t>(Transport::HEADER_SIZE));
    auto unpacked = Transport::unpack(packed);
    ASSERT_TRUE(unpacked.has_value());
    EXPECT_EQ(unpacked->getType(), Transport::ACK);
}
