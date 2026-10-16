#include "modules/lighting/domain/KeyboardStatus.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace ghub::lighting {
namespace {

using Bytes = std::vector<std::uint8_t>;

// Bytes da captura C-00 do G915 (INC-015 parte 2).
TEST(KeyboardStatusTest, ParsesTheCapturedMaximumBrightness)
{
    const Bytes response{0x00, 0x64, 0x05, 0x03};

    const auto limits = parseBrightnessLimits(response);

    ASSERT_TRUE(limits.has_value());
    EXPECT_EQ(limits->maximum, 100);
    EXPECT_EQ(limits->unexplained, (std::array<std::uint8_t, 2>{0x05, 0x03}));
}

TEST(KeyboardStatusTest, ReadsBrightnessAsBigEndian)
{
    const auto limits = parseBrightnessLimits(Bytes{0x01, 0x2C, 0x00, 0x00});

    ASSERT_TRUE(limits.has_value());
    EXPECT_EQ(limits->maximum, 300);
}

TEST(KeyboardStatusTest, RejectsAShortMaximumBrightnessResponse)
{
    const auto limits = parseBrightnessLimits(Bytes{0x00, 0x64, 0x05});

    ASSERT_FALSE(limits.has_value());
    EXPECT_EQ(limits.error().code, shared_kernel::ErrorCode::MalformedData);
}

TEST(KeyboardStatusTest, ParsesTheCapturedCurrentBrightness)
{
    const auto current = parseCurrentBrightness(Bytes{0x00, 0x64});

    ASSERT_TRUE(current.has_value());
    EXPECT_EQ(*current, 100);
}

TEST(KeyboardStatusTest, RejectsAShortCurrentBrightnessResponse)
{
    EXPECT_FALSE(parseCurrentBrightness(Bytes{0x00}).has_value());
    EXPECT_FALSE(parseCurrentBrightness(Bytes{}).has_value());
}

TEST(KeyboardStatusTest, ParsesTheCapturedReportRateBitmapAsPeriods)
{
    const auto support = parseReportRateSupport(Bytes{0x8B});

    ASSERT_TRUE(support.has_value());
    EXPECT_EQ(support->periodsMs(), (std::vector<std::uint8_t>{1, 2, 4, 8}));
    EXPECT_TRUE(support->supports(4));
    EXPECT_FALSE(support->supports(3));
    EXPECT_FALSE(support->supports(0));
}

TEST(KeyboardStatusTest, RejectsAnEmptyReportRateBitmap)
{
    const auto support = parseReportRateSupport(Bytes{0x00});

    ASSERT_FALSE(support.has_value());
    EXPECT_EQ(support.error().code, shared_kernel::ErrorCode::MalformedData);
    EXPECT_FALSE(parseReportRateSupport(Bytes{}).has_value());
}

TEST(KeyboardStatusTest, ParsesTheCapturedCurrentPeriod)
{
    const auto period = parseReportPeriodMs(Bytes{0x01});

    ASSERT_TRUE(period.has_value());
    EXPECT_EQ(*period, 1);
}

TEST(KeyboardStatusTest, RejectsAZeroOrMissingPeriod)
{
    EXPECT_FALSE(parseReportPeriodMs(Bytes{0x00}).has_value());
    EXPECT_FALSE(parseReportPeriodMs(Bytes{}).has_value());
}

} // namespace
} // namespace ghub::lighting
