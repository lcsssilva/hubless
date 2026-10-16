#include "modules/battery/infrastructure/HidppBatteryReader.hpp"
#include "fakes/StatusReadHarness.hpp"
#include "modules/battery/infrastructure/G733BatteryPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace ghub::battery {
namespace {

using platform::hidpp::HidppNodeLocator;
using shared_kernel::ErrorCode;

bool policyAllows(std::uint16_t feature, std::uint8_t function, const std::vector<std::uint8_t>& params = {})
{
    return G733BatteryPolicy{}.isAllowed(feature, function, params);
}

TEST(G733BatteryPolicyTest, AllowsOnlyRootGetFeatureOf1f20AndTheBatteryReadWithoutParams)
{
    EXPECT_TRUE(policyAllows(0x0000, 0, {0x1F, 0x20}));
    EXPECT_TRUE(policyAllows(0x1F20, 0));
}

TEST(G733BatteryPolicyTest, DeniesEverythingElse)
{
    EXPECT_FALSE(policyAllows(0x0000, 0, {0x80, 0x70})) << "Root só para a 0x1f20";
    EXPECT_FALSE(policyAllows(0x0000, 0, {0x1F}));
    EXPECT_FALSE(policyAllows(0x0000, 0, {0x1F, 0x20, 0x00}));
    EXPECT_FALSE(policyAllows(0x0000, 0, {}));
    EXPECT_FALSE(policyAllows(0x1F20, 0, {0x00})) << "sem parâmetros";
    for (std::uint8_t function = 1; function < 16; ++function) {
        EXPECT_FALSE(policyAllows(0x0000, function, {0x1F, 0x20})) << "Root fn " << int{function};
        EXPECT_FALSE(policyAllows(0x1F20, function)) << "0x1f20 fn " << int{function};
    }
    for (const std::uint16_t feature : {std::uint16_t{0x8070}, std::uint16_t{0x8300}, std::uint16_t{0x8310},
                                        std::uint16_t{0x8010}, std::uint16_t{0x0001}, std::uint16_t{0x1000}}) {
        for (std::uint8_t function = 0; function < 16; ++function) {
            EXPECT_FALSE(policyAllows(feature, function)) << std::hex << feature << " fn " << std::dec << int{function};
        }
    }
}

class HidppBatteryReaderTest : public fakes::StatusReadFixture {
protected:
    void SetUp() override
    {
        StatusReadFixture::SetUp();
        // Mapa de features das capturas C-00/C-01 do G733; bateria C-01: 0e ed 01 = 3821 mV.
        device.features = {0x0001, 0x0003, 0x0005, 0x8070, 0x8010, 0x8310, 0x8300, 0x1f20};
        device.responses[{0x1f20, 0}] = {0x0E, 0xED, 0x01};
    }

    bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                      const std::vector<std::uint8_t>& params) const override
    {
        return G733BatteryPolicy{}.isAllowed(featureId, function, params);
    }

    [[nodiscard]] HidppBatteryReader reader()
    {
        return HidppBatteryReader{HidppNodeLocator{root, "/dev"}, clock, opener()};
    }
};

TEST_F(HidppBatteryReaderTest, ReadsTheCapturedBytesWithTheExactRequestSequence)
{
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_TRUE(reading.has_value()) << reading.error().detail;
    EXPECT_EQ(reading->chargeState(), ChargeState::Discharging);
    ASSERT_TRUE(reading->currentLevel().has_value());
    EXPECT_EQ(reading->currentLevel()->percent(), 83) << "3821 mV na curva do G HUB (ADR-0033)";
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 1f20", "1f20 fn0"}));
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw3"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 0xFF) << "índice de dispositivo do G733";
    }
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppBatteryReaderTest, ChargingFlagIsCharging)
{
    device.responses[{0x1f20, 0}] = {0x0F, 0xA0, 0x03};
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_TRUE(reading.has_value()) << reading.error().detail;
    EXPECT_EQ(reading->chargeState(), ChargeState::Charging);
    EXPECT_EQ(reading->currentLevel()->percent(), 100) << "4000 mV acima da curva do G HUB satura em 100 (ADR-0033)";
}

TEST_F(HidppBatteryReaderTest, ErrorFiveMeansTheHeadsetIsOffOrOutOfRange)
{
    device.hidpp20Error = 0x05;
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(reading.error().detail, "headset desligado ou fora de alcance");
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppBatteryReaderTest, OtherDeviceErrorsShowTheirCode)
{
    device.hidpp20Error = 0x07;
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().detail, "o dispositivo respondeu erro HID++ 2.0 (código 0x07)");
}

TEST_F(HidppBatteryReaderTest, TimesOutAfterTwoSecondsWithoutResendingAnything)
{
    device.silent = true;
    auto battery = reader();
    const auto before = clock.now();

    const auto reading = battery.read();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(clock.now() - before, std::chrono::milliseconds{2000});
    EXPECT_EQ(io->written().size(), 1U) << "sem reenvio";
}

TEST_F(HidppBatteryReaderTest, MissingFeatureIsAnErrorAfterOnlyTheRootQuery)
{
    device.features = {0x0001, 0x8070};
    auto battery = reader();

    ASSERT_FALSE(battery.read().has_value());
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 1f20"}));
}

TEST_F(HidppBatteryReaderTest, ZeroedPayloadIsUnknownStateWithoutLevel)
{
    // O relatório longo sempre traz 16 bytes de payload; os ausentes chegam zerados (0 mV, sinalizador 0).
    device.responses[{0x1f20, 0}] = {0x0E};
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(reading->chargeState(), ChargeState::Unknown);
}

TEST_F(HidppBatteryReaderTest, FailsWithoutOpeningWhenTheNodeIsNotFound)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw3");
    auto battery = reader();

    ASSERT_FALSE(battery.read().has_value());
    EXPECT_TRUE(openedPaths.empty());
}

TEST_F(HidppBatteryReaderTest, PropagatesAnOpenFailure)
{
    openFailure = "permissão negada";
    auto battery = reader();

    const auto reading = battery.read();

    ASSERT_FALSE(reading.has_value());
    EXPECT_EQ(reading.error().detail, "permissão negada");
    EXPECT_TRUE(io->written().empty());
}

} // namespace
} // namespace ghub::battery
