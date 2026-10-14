#include "modules/battery/application/GetBatteryStatus.hpp"
#include "fakes/FakeBatterySource.hpp"

#include <gtest/gtest.h>

#include <vector>

using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::DeviceBatteryStatus;
using ghub::battery::GetBatteryStatus;
using ghub::battery::SourcedBatteryReading;
using ghub::device_inventory::DeviceModel;
using ghub::device_inventory::UsbId;

namespace {

constexpr UsbId g915{.vendorId = 0x046D, .productId = 0x407C};
constexpr UsbId g703{.vendorId = 0x046D, .productId = 0x4086};
constexpr UsbId g733{.vendorId = 0x046D, .productId = 0x0AFE};

BatteryReading discharging(int percent)
{
    return BatteryReading{ChargeState::Discharging, BatteryLevel::fromPercent(percent).value()};
}

class GetBatteryStatusTest : public ::testing::Test {
protected:
    std::vector<DeviceBatteryStatus> run(std::vector<SourcedBatteryReading> readings)
    {
        source.setResult(std::move(readings));
        return GetBatteryStatus{{&source}}.execute();
    }

    ghub::fakes::FakeBatterySource source;
    ghub::fakes::FakeBatterySource otherSource;
};

} // namespace

TEST_F(GetBatteryStatusTest, ReportsAllModels_InFixedOrder)
{
    auto statuses = run({{.device = g703, .reading = discharging(95)}, {.device = g915, .reading = discharging(44)}});

    ASSERT_EQ(statuses.size(), 3U);
    EXPECT_EQ(statuses[0].model, DeviceModel::G915);
    EXPECT_EQ(std::get<BatteryReading>(statuses[0].state), discharging(44));
    EXPECT_EQ(statuses[1].model, DeviceModel::G703);
    EXPECT_EQ(std::get<BatteryReading>(statuses[1].state), discharging(95));
}

TEST_F(GetBatteryStatusTest, MarksNotDetected_WhenDeviceAbsent)
{
    auto statuses = run({{.device = g915, .reading = discharging(44)}});

    ASSERT_EQ(statuses.size(), 3U);
    EXPECT_EQ(statuses[1].model, DeviceModel::G703);
    EXPECT_TRUE(std::holds_alternative<ghub::battery::NotDetected>(statuses[1].state));
}

TEST_F(GetBatteryStatusTest, IgnoresUnknownProducts)
{
    const UsbId receiver{.vendorId = 0x046D, .productId = 0xC547};

    auto statuses =
        run({{.device = receiver, .reading = discharging(10)}, {.device = g703, .reading = discharging(95)}});

    ASSERT_EQ(statuses.size(), 3U);
    EXPECT_TRUE(std::holds_alternative<ghub::battery::NotDetected>(statuses[0].state));
    EXPECT_EQ(std::get<BatteryReading>(statuses[1].state), discharging(95));
}

TEST_F(GetBatteryStatusTest, UsesFirstReport_WhenDeviceReportedTwice)
{
    auto statuses = run({{.device = g915, .reading = discharging(44)}, {.device = g915, .reading = discharging(80)}});

    EXPECT_EQ(std::get<BatteryReading>(statuses[0].state), discharging(44));
}

TEST_F(GetBatteryStatusTest, MarksUnreadable_OnlyForDeviceWithFailedReading)
{
    using ghub::shared_kernel::DomainError;
    using ghub::shared_kernel::ErrorCode;

    auto statuses = run({
        {.device = g915, .reading = ghub::shared_kernel::fail(ErrorCode::MalformedData, "capacity")},
        {.device = g703, .reading = discharging(95)},
    });

    ASSERT_EQ(statuses.size(), 3U);
    ASSERT_TRUE(std::holds_alternative<DomainError>(statuses[0].state));
    EXPECT_EQ(std::get<DomainError>(statuses[0].state).code, ErrorCode::MalformedData);
    EXPECT_EQ(std::get<BatteryReading>(statuses[1].state), discharging(95));
}

TEST_F(GetBatteryStatusTest, MarksAllUnreadable_WhenSourceFails)
{
    using ghub::shared_kernel::DomainError;
    using ghub::shared_kernel::ErrorCode;
    source.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "/sys/class/power_supply"));

    auto statuses = GetBatteryStatus{{&source}}.execute();

    ASSERT_EQ(statuses.size(), 3U);
    for (const auto& status : statuses) {
        ASSERT_TRUE(std::holds_alternative<DomainError>(status.state));
        EXPECT_EQ(std::get<DomainError>(status.state).code, ErrorCode::BackendUnavailable);
    }
}

// Regressão (2026-10-13): G703 no cabo → entrada sem fio "Unknown" (ordenada antes) + entrada USB
// "Charging". A leitura com estado conhecido deve prevalecer.
TEST_F(GetBatteryStatusTest, PrefersKnownChargeState_WhenDeviceReportedTwice)
{
    const UsbId g703Cable{.vendorId = 0x046D, .productId = 0xC090};
    const BatteryReading charging{ChargeState::Charging, BatteryLevel::fromPercent(96).value()};

    auto statuses = run({
        {.device = g703, .reading = BatteryReading{ChargeState::Unknown, BatteryLevel::fromPercent(96).value()}},
        {.device = g703Cable, .reading = charging},
    });

    EXPECT_EQ(std::get<BatteryReading>(statuses[1].state), charging);
}

TEST_F(GetBatteryStatusTest, ReportsG733_AfterG703)
{
    auto statuses = run({{.device = g733, .reading = discharging(60)}});

    ASSERT_EQ(statuses.size(), 3U);
    EXPECT_EQ(statuses[0].model, DeviceModel::G915);
    EXPECT_EQ(statuses[1].model, DeviceModel::G703);
    EXPECT_EQ(statuses[2].model, DeviceModel::G733);
    EXPECT_EQ(std::get<BatteryReading>(statuses[2].state), discharging(60));
}

TEST_F(GetBatteryStatusTest, CombinesSources)
{
    source.setResult(std::vector<SourcedBatteryReading>{{.device = g915, .reading = discharging(44)}});
    otherSource.setResult(std::vector<SourcedBatteryReading>{{.device = g733, .reading = discharging(60)}});

    auto statuses = GetBatteryStatus{{&source, &otherSource}}.execute();

    ASSERT_EQ(statuses.size(), 3U);
    EXPECT_EQ(std::get<BatteryReading>(statuses[0].state), discharging(44));
    EXPECT_TRUE(std::holds_alternative<ghub::battery::NotDetected>(statuses[1].state));
    EXPECT_EQ(std::get<BatteryReading>(statuses[2].state), discharging(60));
}

TEST_F(GetBatteryStatusTest, MarksUnreadable_WhenNoReadingAndSourceFailed)
{
    using ghub::shared_kernel::DomainError;
    using ghub::shared_kernel::ErrorCode;
    source.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "/sys/class/power_supply"));
    otherSource.setResult(std::vector<SourcedBatteryReading>{{.device = g733, .reading = discharging(60)}});

    auto statuses = GetBatteryStatus{{&source, &otherSource}}.execute();

    ASSERT_EQ(statuses.size(), 3U);
    ASSERT_TRUE(std::holds_alternative<DomainError>(statuses[0].state));
    EXPECT_EQ(std::get<DomainError>(statuses[0].state).code, ErrorCode::BackendUnavailable);
    ASSERT_TRUE(std::holds_alternative<DomainError>(statuses[1].state));
    EXPECT_EQ(std::get<BatteryReading>(statuses[2].state), discharging(60));
}
