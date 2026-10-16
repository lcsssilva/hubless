#include "modules/battery/domain/LowBatteryAlert.hpp"

#include <gtest/gtest.h>

using ghub::battery::BatteryLevel;
using ghub::battery::ChargeState;
using ghub::battery::LowBatteryAlert;
using ghub::battery::LowBatteryThreshold;
using ghub::shared_kernel::ErrorCode;

namespace {
BatteryLevel level(int percent)
{
    return *BatteryLevel::fromPercent(percent);
}

LowBatteryAlert enabledAlert(int threshold = 15)
{
    LowBatteryAlert alert;
    alert.setThreshold(*LowBatteryThreshold::create(threshold));
    alert.setEnabled(true);
    return alert;
}
} // namespace

TEST(LowBatteryThreshold, AcceptsFiveToFifty)
{
    EXPECT_TRUE(LowBatteryThreshold::create(5).has_value());
    EXPECT_TRUE(LowBatteryThreshold::create(50).has_value());
    EXPECT_EQ(LowBatteryThreshold::create(4).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(LowBatteryThreshold::create(51).error().code, ErrorCode::InvalidArgument);
}

TEST(LowBatteryAlert, IsDisabledByDefaultWithThresholdFifteen)
{
    LowBatteryAlert alert;
    EXPECT_FALSE(alert.enabled());
    EXPECT_EQ(alert.threshold().percent(), 15);
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(3)));
}

TEST(LowBatteryAlert, NotifiesOnceWhenReachingTheThreshold)
{
    auto alert = enabledAlert();
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(16)));
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(15)));
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(14)));
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(10)));
}

TEST(LowBatteryAlert, ChargingRearmsAndNeverNotifies)
{
    auto alert = enabledAlert();
    EXPECT_FALSE(alert.observe(ChargeState::Charging, level(10)));
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(10)));
    EXPECT_FALSE(alert.observe(ChargeState::Charging, level(11)));
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(10)));
}

TEST(LowBatteryAlert, RearmsOnlyFivePointsAboveTheThreshold)
{
    auto alert = enabledAlert();
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(15)));
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(19)));
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(14))); // 19 não rearmou
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(20))); // rearma aqui
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(15)));
}

TEST(LowBatteryAlert, UnknownLevelDoesNothing)
{
    auto alert = enabledAlert();
    EXPECT_FALSE(alert.observe(ChargeState::Unknown, std::nullopt));
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(5)));
}

TEST(LowBatteryAlert, DisabledNeverNotifiesAndEnablingLowStillNotifiesOnce)
{
    auto alert = enabledAlert();
    alert.setEnabled(false);
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(5)));
    alert.setEnabled(true);
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(5)));
    EXPECT_FALSE(alert.observe(ChargeState::Discharging, level(5)));
}

TEST(LowBatteryAlert, FullChargeRearms)
{
    auto alert = enabledAlert(50);
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(50)));
    EXPECT_FALSE(alert.observe(ChargeState::Full, level(100)));
    EXPECT_TRUE(alert.observe(ChargeState::Discharging, level(40)));
}
