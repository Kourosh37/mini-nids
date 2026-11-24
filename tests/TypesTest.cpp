#include <gtest/gtest.h>
#include <limits>
#include <mininids/common/Types.hpp>
using namespace mininids;
TEST(Types, FormatsNetworkValuesAndNanoseconds) {
    EXPECT_EQ(ip_string(0xc0000201), "192.0.2.1");
    EXPECT_EQ(mac_string({0, 1, 2, 0xaa, 0xbb, 0xff}), "00:01:02:aa:bb:ff");
    EXPECT_EQ(timestamp_string(second + 42), "1.000000042");
    EXPECT_EQ(timestamp_utc(second + 42), "1970-01-01T00:00:01.000000042Z");
    EXPECT_EQ(timestamp_utc(1732345678123456789LL), "2024-11-23T07:07:58.123456789Z");
    EXPECT_EQ(timestamp_utc(1732345678123456000LL, false), "2024-11-23T07:07:58.123456Z");
    EXPECT_EQ(timestamp_utc(951782400LL * second), "2000-02-29T00:00:00.000000000Z");
    EXPECT_THROW(timestamp_utc(-1), Error);
}
TEST(Types, DurationsKeepExactUnitsAndSignedDelays) {
    EXPECT_EQ(duration_string(0), "0 ns");
    EXPECT_EQ(duration_string(999), "999 ns");
    EXPECT_EQ(duration_string(1234), "1.234 \xC2\xB5s");
    EXPECT_EQ(duration_string(1000001), "1.000001 ms");
    EXPECT_EQ(duration_string(second + 42), "1.000000042 s");
    EXPECT_EQ(duration_string(60 * second), "1 min");
    EXPECT_EQ(duration_string(3601 * second), "1 h 1 s");
    EXPECT_EQ(duration_string(86400 * second + 1234567), "1 d 0.001234567 s");
    EXPECT_EQ(duration_string(-1500000), "-1.5 ms");
    EXPECT_FALSE(duration_string(std::numeric_limits<Timestamp>::min()).empty());
}
TEST(Types, RejectsOutOfBoundsReads) {
    const std::array<std::uint8_t, 2> data{1, 2};
    EXPECT_EQ(be16(data, 0), 258);
    EXPECT_THROW(be16(data, 1), Error);
    EXPECT_THROW(be32(data, 0), Error);
}
