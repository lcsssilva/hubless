#include "modules/headset/domain/HeadsetStatus.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace ghub::headset {
namespace {

using Bytes = std::vector<std::uint8_t>;

// Bytes das capturas C-00/C-01/C-03 do G733 (INC-015 parte 2): 0x8310 fn0 -> 0a 0c 01.
TEST(HeadsetStatusTest, ParsesTheCapturedEqualizerInfo)
{
    const auto info = parseEqualizerInfo(Bytes{0x0A, 0x0C, 0x01});

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->bandCount, 10);
    EXPECT_EQ(info->unexplained, (std::array<std::uint8_t, 2>{0x0C, 0x01}));
}

TEST(HeadsetStatusTest, RejectsAShortEqualizerInfoOrZeroBands)
{
    EXPECT_FALSE(parseEqualizerInfo(Bytes{0x0A, 0x0C}).has_value());
    EXPECT_FALSE(parseEqualizerInfo(Bytes{}).has_value());
    const auto zero = parseEqualizerInfo(Bytes{0x00, 0x0C, 0x01});
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error().code, shared_kernel::ErrorCode::MalformedData);
}

TEST(HeadsetStatusTest, KeepsTheFirstFourBytesOfTheUninterpretedReadRaw)
{
    const auto raw = parseUninterpretedRead(Bytes{0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    const auto other = parseUninterpretedRead(Bytes{0x01, 0x02, 0x03, 0x04, 0x05});

    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(raw->bytes, (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(other->bytes, (std::array<std::uint8_t, 4>{1, 2, 3, 4}));
}

TEST(HeadsetStatusTest, RejectsAShortUninterpretedRead)
{
    const auto raw = parseUninterpretedRead(Bytes{0x00, 0x00, 0x00});

    ASSERT_FALSE(raw.has_value());
    EXPECT_EQ(raw.error().code, shared_kernel::ErrorCode::MalformedData);
}

} // namespace
} // namespace ghub::headset
