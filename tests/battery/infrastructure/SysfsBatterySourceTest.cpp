#include "FakePowerSupplyTree.hpp"

#include "modules/battery/infrastructure/SysfsBatterySource.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

using ghub::battery::BatteryLevel;
using ghub::battery::BatteryReading;
using ghub::battery::ChargeState;
using ghub::battery::SysfsBatterySource;
using ghub::battery::test::FakePowerSupplyTree;
using ghub::battery::test::hidppAttributes;
using ghub::device_inventory::UsbId;

namespace {

constexpr auto g915Hid = "0003:046D:407C.000A";

class SysfsBatterySourceTest : public ::testing::Test {
protected:
    // Lê a única entrada esperada na árvore.
    ghub::shared_kernel::Result<BatteryReading> readSingle()
    {
        auto readings = source.readAll();
        EXPECT_TRUE(readings.has_value());
        EXPECT_EQ(readings.value_or(std::vector<ghub::battery::SourcedBatteryReading>{}).size(), 1U);
        if (!readings.has_value() || readings->size() != 1U) {
            return ghub::shared_kernel::fail(ghub::shared_kernel::ErrorCode::InvalidArgument, "teste");
        }
        return readings->front().reading;
    }

    FakePowerSupplyTree tree;
    SysfsBatterySource source{tree.classRoot()};
};

} // namespace

TEST_F(SysfsBatterySourceTest, ReadsDevice_IdentifiedByHidPath)
{
    tree.addEntry("hidpp_battery_7", "0003:046D:407C.000A", hidppAttributes("Discharging", "44"));

    auto readings = source.readAll();

    ASSERT_TRUE(readings.has_value());
    ASSERT_EQ(readings->size(), 1U);
    EXPECT_EQ(readings->front().device, (UsbId{.vendorId = 0x046D, .productId = 0x407C}));
    ASSERT_TRUE(readings->front().reading.has_value());
    EXPECT_EQ(*readings->front().reading,
              (BatteryReading{ChargeState::Discharging, BatteryLevel::fromPercent(44).value()}));
}

TEST_F(SysfsBatterySourceTest, MapsEveryAbiChargeState)
{
    const std::vector<std::pair<std::string, ChargeState>> cases{
        {"Unknown", ChargeState::Unknown},
        {"Charging", ChargeState::Charging},
        {"Discharging", ChargeState::Discharging},
        {"Not charging", ChargeState::NotCharging},
        {"Full", ChargeState::Full},
    };
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Unknown", "50"));
    for (const auto& [text, expected] : cases) {
        tree.writeAttribute("hidpp_battery_0", "status", text + "\n");

        auto reading = readSingle();

        ASSERT_TRUE(reading.has_value()) << text;
        EXPECT_EQ(reading->chargeState(), expected) << text;
    }
}

TEST_F(SysfsBatterySourceTest, ReportsMalformed_WhenStatusUnrecognized)
{
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Bogus", "50"));

    auto reading = readSingle();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ghub::shared_kernel::ErrorCode::MalformedData);
}

TEST_F(SysfsBatterySourceTest, ReadsWithoutLevel_WhenCapacityMissing)
{
    tree.addEntry("hidpp_battery_0", g915Hid, {{"type", "Battery"}, {"scope", "Device"}, {"status", "Discharging"}});

    auto reading = readSingle();

    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(reading->chargeState(), ChargeState::Discharging);
    EXPECT_FALSE(reading->currentLevel().has_value());
}

TEST_F(SysfsBatterySourceTest, ReportsMalformed_WhenCapacityNotNumeric)
{
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Discharging", "abc"));
    for (const auto* capacity : {"abc", "44%", ""}) {
        tree.writeAttribute("hidpp_battery_0", "capacity", std::string{capacity} + "\n");

        auto reading = readSingle();

        ASSERT_FALSE(reading.has_value()) << capacity;
        EXPECT_EQ(reading.error().code, ghub::shared_kernel::ErrorCode::MalformedData) << capacity;
    }
}

TEST_F(SysfsBatterySourceTest, ReportsMalformed_WhenCapacityOutOfRange)
{
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Discharging", "150"));

    auto reading = readSingle();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ghub::shared_kernel::ErrorCode::MalformedData);
}

TEST_F(SysfsBatterySourceTest, SkipsEntries_ThatAreNotPeripheralBatteries)
{
    auto mains = hidppAttributes("Discharging", "50");
    mains["type"] = "Mains";
    auto systemScope = hidppAttributes("Discharging", "50");
    systemScope["scope"] = "System";
    auto noScope = hidppAttributes("Discharging", "50");
    noScope.erase("scope");
    tree.addEntry("AC", "0003:046D:407C.0001", mains);
    tree.addEntry("BAT0", "0003:046D:407C.0002", systemScope);
    tree.addEntry("BAT1", "0003:046D:407C.0003", noScope);

    auto readings = source.readAll();

    ASSERT_TRUE(readings.has_value());
    EXPECT_TRUE(readings->empty());
}

TEST_F(SysfsBatterySourceTest, SkipsEntries_WithUnidentifiableDevice)
{
    tree.addEntry("hidpp_battery_0", "card0", hidppAttributes("Discharging", "50"));
    tree.addEntry("hidpp_battery_1", "0003:046D:ZZZZ.0001", hidppAttributes("Discharging", "50"));
    tree.addEntry("hidpp_battery_2", "0003:046D:407C", hidppAttributes("Discharging", "50"));
    tree.addEntry("hidpp_battery_4", "0003:046D:407C.ZZ", hidppAttributes("Discharging", "50"));
    tree.addEntry("hidpp_battery_3", "0003:046D:4086.0009", hidppAttributes("Discharging", "95"));
    std::filesystem::remove(tree.classRoot() / "hidpp_battery_3" / "device");

    auto readings = source.readAll();

    ASSERT_TRUE(readings.has_value());
    EXPECT_TRUE(readings->empty());
}

TEST_F(SysfsBatterySourceTest, FailsUnavailable_WhenRootMissing)
{
    SysfsBatterySource missing{tree.classRoot() / "inexistente"};

    auto readings = missing.readAll();

    ASSERT_FALSE(readings.has_value());
    EXPECT_EQ(readings.error().code, ghub::shared_kernel::ErrorCode::BackendUnavailable);
}

TEST_F(SysfsBatterySourceTest, ReturnsEntries_SortedByName)
{
    for (const auto* name : {"hidpp_battery_2", "hidpp_battery_0", "hidpp_battery_3", "hidpp_battery_1"}) {
        const std::string suffix{std::string_view{name}.back()};
        tree.addEntry(name, "0003:046D:407C.000" + suffix, hidppAttributes("Discharging", "1" + suffix));
    }

    auto readings = source.readAll();

    ASSERT_TRUE(readings.has_value());
    ASSERT_EQ(readings->size(), 4U);
    for (std::size_t index = 0; index < readings->size(); ++index) {
        const auto& entry = readings->at(index);
        ASSERT_TRUE(entry.reading.has_value());
        EXPECT_EQ(entry.reading->currentLevel(), BatteryLevel::fromPercent(10 + static_cast<int>(index)).value());
    }
}

TEST_F(SysfsBatterySourceTest, ReportsMalformed_WhenAttributeTooLarge)
{
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Discharging", "50"));
    tree.writeAttribute("hidpp_battery_0", "capacity", std::string(4097, '0'));

    auto reading = readSingle();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ghub::shared_kernel::ErrorCode::MalformedData);
}

TEST_F(SysfsBatterySourceTest, ReportsMalformed_WhenStatusMissing)
{
    tree.addEntry("hidpp_battery_0", g915Hid, {{"type", "Battery"}, {"scope", "Device"}, {"capacity", "50"}});

    auto reading = readSingle();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ghub::shared_kernel::ErrorCode::MalformedData);
}

TEST_F(SysfsBatterySourceTest, FailsUnavailable_WhenRootNotReadable)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "root ignora permissões de diretório; rode os testes como usuário comum";
    }
    tree.addEntry("hidpp_battery_0", g915Hid, hidppAttributes("Discharging", "44"));
    std::filesystem::permissions(tree.classRoot(), std::filesystem::perms::none);

    auto readings = source.readAll();

    std::filesystem::permissions(tree.classRoot(), std::filesystem::perms::owner_all);
    ASSERT_FALSE(readings.has_value());
    EXPECT_EQ(readings.error().code, ghub::shared_kernel::ErrorCode::BackendUnavailable);
}
