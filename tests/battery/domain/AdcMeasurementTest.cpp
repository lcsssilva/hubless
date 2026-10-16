#include "modules/battery/domain/AdcMeasurement.hpp"

#include <gtest/gtest.h>

#include <vector>

using ghub::battery::capacityFromMillivolts;
using ghub::battery::ChargeState;
using ghub::battery::readingFromAdc;
using ghub::shared_kernel::ErrorCode;

// Tabela do kernel Linux (hid-logitech-hidpp.c, hidpp20_map_adc_measurement_1f20_capacity): o primeiro degrau cuja
// tensão é <= à medida dá 100 - índice.
TEST(AdcMeasurementTest, CapacityFollowsTheKernelTablePoints)
{
    EXPECT_EQ(capacityFromMillivolts(4030), 100);
    EXPECT_EQ(capacityFromMillivolts(4500), 100);
    EXPECT_EQ(capacityFromMillivolts(4029), 99);
    EXPECT_EQ(capacityFromMillivolts(3820), 80);
    EXPECT_EQ(capacityFromMillivolts(3819), 79);
    EXPECT_EQ(capacityFromMillivolts(3529), 6);
    EXPECT_EQ(capacityFromMillivolts(3525), 5);
    EXPECT_EQ(capacityFromMillivolts(3499), 4);
    EXPECT_EQ(capacityFromMillivolts(3399), 1);
}

TEST(AdcMeasurementTest, BelowTheLastStepOrZeroIsZeroPercent)
{
    EXPECT_EQ(capacityFromMillivolts(3398), 0);
    EXPECT_EQ(capacityFromMillivolts(3000), 0);
    EXPECT_EQ(capacityFromMillivolts(0), 0);
}

TEST(AdcMeasurementTest, TheCapturedMeasurementsGiveWhatTheKernelWould)
{
    // C-01 do G733: 0e ed 01 = 3821 mV, 0e dc 01 = 3804, 0e ca 01 = 3786, 0e f1 01 = 3825.
    EXPECT_EQ(capacityFromMillivolts(3821), 80);
    EXPECT_EQ(capacityFromMillivolts(3804), 77);
    EXPECT_EQ(capacityFromMillivolts(3786), 75);
    EXPECT_EQ(capacityFromMillivolts(3825), 80);
}

TEST(AdcMeasurementTest, ReadingFromTheCapturedBytes)
{
    const auto reading = readingFromAdc(std::vector<std::uint8_t>{0x0E, 0xED, 0x01});

    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(reading->chargeState(), ChargeState::Discharging);
    ASSERT_TRUE(reading->currentLevel().has_value());
    EXPECT_EQ(reading->currentLevel()->percent(), 80);
}

TEST(AdcMeasurementTest, FlagsMapAsInTheKernel)
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
