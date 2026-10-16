#include "modules/lighting/infrastructure/HidppKeyboardStatusSource.hpp"
#include "fakes/StatusReadHarness.hpp"
#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace ghub::lighting::infrastructure {
namespace {

using platform::hidpp::HidppNodeLocator;
using shared_kernel::ErrorCode;

class HidppKeyboardStatusSourceTest : public fakes::StatusReadFixture {
protected:
    void SetUp() override
    {
        StatusReadFixture::SetUp();
        // Bytes das capturas C-00 do G915 (INC-015 parte 2).
        device.features = {0x0001, 0x8040, 0x8060};
        device.responses[{0x8040, 0}] = {0x00, 0x64, 0x05, 0x03};
        device.responses[{0x8040, 1}] = {0x00, 0x64};
        device.responses[{0x8060, 0}] = {0x8B};
        device.responses[{0x8060, 1}] = {0x01};
    }

    bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                      const std::vector<std::uint8_t>& params) const override
    {
        return LightingHidppPolicy{}.isAllowed(featureId, function, params);
    }

    [[nodiscard]] HidppKeyboardStatusSource source()
    {
        return HidppKeyboardStatusSource{HidppNodeLocator{root, "/dev"}, clock, opener()};
    }
};

TEST_F(HidppKeyboardStatusSourceTest, ReadsTheCapturedBytesWithTheExactRequestSequence)
{
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    ASSERT_TRUE(status->brightnessLimits.has_value());
    EXPECT_EQ(status->brightnessLimits->maximum, 100);
    EXPECT_EQ(status->brightnessLimits->unexplained, (std::array<std::uint8_t, 2>{0x05, 0x03}));
    EXPECT_EQ(status->currentBrightness, 100);
    ASSERT_TRUE(status->reportRateSupport.has_value());
    EXPECT_EQ(status->reportRateSupport->periodsMs(), (std::vector<std::uint8_t>{1, 2, 4, 8}));
    EXPECT_EQ(status->currentPeriodMs, 1);
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw7"}));
    EXPECT_EQ(sequence(),
              (std::vector<std::string>{"root 8040", "8040 fn0", "8040 fn1", "root 8060", "8060 fn0", "8060 fn1"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 1) << "índice do dispositivo atrás do receptor";
    }
}

TEST_F(HidppKeyboardStatusSourceTest, KeepsTheHidrawOpenOnlyWhileReading)
{
    auto reader = source();
    ASSERT_EQ(openHandles, 0);

    ASSERT_TRUE(reader.read().has_value());

    EXPECT_EQ(openHandles, 0);
    EXPECT_EQ(openedPaths.size(), 1U);
}

TEST_F(HidppKeyboardStatusSourceTest, AbsentFeatureIsNotInformedWithoutFailingTheRest)
{
    device.features = {0x0001, 0x8060};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    EXPECT_FALSE(status->brightnessLimits.has_value());
    EXPECT_FALSE(status->currentBrightness.has_value());
    EXPECT_EQ(status->currentPeriodMs, 1);
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8040", "root 8060", "8060 fn0", "8060 fn1"}));
}

TEST_F(HidppKeyboardStatusSourceTest, ShowsTheDeviceErrorCode)
{
    device.hidpp20Error = 0x07;
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(status.error().detail, "o dispositivo respondeu erro HID++ 2.0 (código 0x07)");
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppKeyboardStatusSourceTest, TimesOutAfterTwoSecondsWithoutResendingAnything)
{
    device.silent = true;
    auto reader = source();
    const auto before = clock.now();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(status.error().detail, "sem resposta (dispositivo dormindo ou desligado?)");
    EXPECT_EQ(clock.now() - before, std::chrono::milliseconds{2000});
    EXPECT_EQ(io->written().size(), 1U) << "sem reenvio";
}

TEST_F(HidppKeyboardStatusSourceTest, RejectsAMalformedResponse)
{
    device.responses[{0x8060, 0}] = {0x00};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::MalformedData);
}

TEST_F(HidppKeyboardStatusSourceTest, FailsWithoutOpeningWhenTheNodeIsNotFound)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw7");
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_TRUE(openedPaths.empty());
}

TEST_F(HidppKeyboardStatusSourceTest, PropagatesAnOpenFailure)
{
    openFailure = "permissão negada";
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().detail, "permissão negada");
    EXPECT_TRUE(io->written().empty());
}

} // namespace
} // namespace ghub::lighting::infrastructure
