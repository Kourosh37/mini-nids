#include <gtest/gtest.h>
#include <mininids/common/Types.hpp>
using namespace mininids;
TEST(Types, FormatsNetworkValuesAndNanoseconds) {
    EXPECT_EQ(ip_string(0xc0000201), "192.0.2.1");
    EXPECT_EQ(mac_string({0, 1, 2, 0xaa, 0xbb, 0xff}), "00:01:02:aa:bb:ff");
    EXPECT_EQ(timestamp_string(second + 42), "1.000000042");
}
TEST(Types, RejectsOutOfBoundsReads) {
    const std::array<std::uint8_t, 2> data{1, 2};
    EXPECT_EQ(be16(data, 0), 258);
    EXPECT_THROW(be16(data, 1), Error);
    EXPECT_THROW(be32(data, 0), Error);
}
