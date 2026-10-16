#include "modules/battery/domain/HeadsetBroadcast.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

using ghub::battery::isHidppBroadcastFromHeadset;

namespace {

TEST(HeadsetBroadcast, PowerOnEventIsBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0xff, 0x08, 0x00, 0x0e, 0xf1, 0x01};
    EXPECT_TRUE(isHidppBroadcastFromHeadset(r));
}

TEST(HeadsetBroadcast, PowerOffEventIsBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0xff, 0x08, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(isHidppBroadcastFromHeadset(r));
}

TEST(HeadsetBroadcast, ResponseToOwnRequestIsNotBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0xff, 0x08, 0x0b, 0x0e, 0xe6, 0x01};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(r));
}

TEST(HeadsetBroadcast, ErrorReportIsNotBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0xff, 0xff, 0x08, 0x0b, 0x05};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(r));
}

TEST(HeadsetBroadcast, ShortReportIsNotBroadcast)
{
    const std::array<std::uint8_t, 7> r{0x10, 0xff, 0x08, 0x00, 0x0e, 0xf1, 0x01};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(r));
    const std::array<std::uint8_t, 3> tiny{0x11, 0xff, 0x08};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(tiny));
}

TEST(HeadsetBroadcast, OtherDeviceIndexIsNotBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0x01, 0x08, 0x00, 0x0e, 0xf1, 0x01};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(r));
}

TEST(HeadsetBroadcast, RootFeatureIndexIsNotBroadcast)
{
    const std::array<std::uint8_t, 20> r{0x11, 0xff, 0x00, 0x00, 0x0e, 0xf1, 0x01};
    EXPECT_FALSE(isHidppBroadcastFromHeadset(r));
}

} // namespace
