#include "modules/battery/domain/AdcMeasurement.hpp"

#include <gtest/gtest.h>

#include <vector>

using ghub::battery::capacityFromMillivolts;
using ghub::battery::ChargeState;
using ghub::battery::readingFromAdc;
using ghub::shared_kernel::ErrorCode;

// Curva e algoritmo do G HUB (ADR-0033): valores calculados e validados contra telas do G HUB.
TEST(AdcMeasurementTest, CapacityFollowsTheGHubCurve)
{
    EXPECT_EQ(capacityFromMillivolts(0), 0);
    EXPECT_EQ(capacityFromMillivolts(3000), 0);
    EXPECT_EQ(capacityFromMillivolts(3460), 0);
    EXPECT_EQ(capacityFromMillivolts(3461), 0);
    EXPECT_EQ(capacityFromMillivolts(3600), 4);
    EXPECT_EQ(capacityFromMillivolts(3700), 23);
    EXPECT_EQ(capacityFromMillivolts(3786), 63);
    EXPECT_EQ(capacityFromMillivolts(3788), 64);
    EXPECT_EQ(capacityFromMillivolts(3792), 67);
    EXPECT_EQ(capacityFromMillivolts(3804), 75);
    EXPECT_EQ(capacityFromMillivolts(3815), 81);
    EXPECT_EQ(capacityFromMillivolts(3816), 82);
    EXPECT_EQ(capacityFromMillivolts(3821), 83);
    EXPECT_EQ(capacityFromMillivolts(3830), 88);
    EXPECT_EQ(capacityFromMillivolts(3867), 99);
    EXPECT_EQ(capacityFromMillivolts(3870), 100);
    EXPECT_EQ(capacityFromMillivolts(3871), 100);
    EXPECT_EQ(capacityFromMillivolts(4200), 100);
}

// C-00/C-01 do G733: o G HUB mostrou 3792 mV -> 67 %, 3788 -> 64 %, 3786 -> 63 %. O 67 % de 3792 mV só sai usando a
// primeira linha entre as de tensão repetida (com a segunda daria 66 %).
TEST(AdcMeasurementTest, MatchesWhatGHubShowedInCapturesC00AndC01)
{
    EXPECT_EQ(capacityFromMillivolts(3792), 67);
    EXPECT_EQ(capacityFromMillivolts(3788), 64);
    EXPECT_EQ(capacityFromMillivolts(3786), 63);
}

TEST(AdcMeasurementTest, ReadingFromTheCapturedBytes)
{
    const auto reading = readingFromAdc(std::vector<std::uint8_t>{0x0E, 0xED, 0x01});

    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(reading->chargeState(), ChargeState::Discharging);
    ASSERT_TRUE(reading->currentLevel().has_value());
    EXPECT_EQ(reading->currentLevel()->percent(), 83);
}

TEST(AdcMeasurementTest, FlagsMapToChargeStates)
{
    EXPECT_EQ(readingFromAdc(std::vector<std::uint8_t>{0x0F, 0x00, 0x03})->chargeState(), ChargeState::Charging);
    EXPECT_EQ(readingFromAdc(std::vector<std::uint8_t>{0x0F, 0x00, 0x07})->chargeState(), ChargeState::Full);
    EXPECT_EQ(readingFromAdc(std::vector<std::uint8_t>{0x0E, 0xED, 0x01})->chargeState(), ChargeState::Discharging);
}

TEST(AdcMeasurementTest, UnknownFlagsGiveUnknownStateWithoutAnExposedLevel)
{
    for (const std::uint8_t flags : {std::uint8_t{0x0F}, std::uint8_t{0x00}, std::uint8_t{0x02}, std::uint8_t{0xFF}}) {
        const auto reading = readingFromAdc(std::vector<std::uint8_t>{0x0E, 0xED, flags});
        ASSERT_TRUE(reading.has_value());
        EXPECT_EQ(reading->chargeState(), ChargeState::Unknown) << int{flags};
        EXPECT_FALSE(reading->currentLevel().has_value());
    }
}

TEST(AdcMeasurementTest, ShortResponseIsMalformed)
{
    const auto reading = readingFromAdc(std::vector<std::uint8_t>{0x0E, 0xED});

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ErrorCode::MalformedData);
}
