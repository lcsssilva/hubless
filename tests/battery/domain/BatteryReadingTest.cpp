#include "modules/battery/domain/BatteryReading.hpp"

#include <gtest/gtest.h>

#include <optional>

using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;

namespace {

BatteryLevel level(int percent)
{
    return BatteryLevel::fromPercent(percent).value();
}

} // namespace

TEST(BatteryReading, ExposesLevel_WhenStateKnown)
{
    const BatteryReading reading{ChargeState::Discharging, level(44)};

    EXPECT_EQ(reading.currentLevel(), std::optional{level(44)});
    EXPECT_EQ(reading.chargeState(), ChargeState::Discharging);
}

TEST(BatteryReading, HidesLevel_WhenStateUnknown)
{
    const BatteryReading reading{ChargeState::Unknown, level(44)};

    EXPECT_FALSE(reading.currentLevel().has_value());
    EXPECT_EQ(reading.chargeState(), ChargeState::Unknown);
}
