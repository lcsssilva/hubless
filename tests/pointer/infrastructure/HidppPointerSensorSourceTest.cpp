#include "modules/pointer/infrastructure/HidppPointerSensorSource.hpp"
#include "fakes/StatusReadHarness.hpp"
#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace ghub::pointer::infrastructure {
namespace {

using platform::hidpp::HidppNodeLocator;
using shared_kernel::ErrorCode;

class HidppPointerSensorSourceTest : public fakes::StatusReadFixture {
protected:
    void SetUp() override
    {
        StatusReadFixture::SetUp();
        // Bytes das capturas C-00 do G703 (INC-015 parte 2).
        device.features = {0x0001, 0x2201, 0x8060};
        device.responses[{0x2201, 1}] = {0x00, 0x00, 0x64, 0xE0, 0x32, 0x64, 0x00};
        device.responses[{0x2201, 2}] = {0x00, 0x1B, 0x8A, 0x03, 0x20};
        device.responses[{0x8060, 0}] = {0x8B};
        device.responses[{0x8060, 1}] = {0x01};
    }

    [[nodiscard]] std::size_t paramCount(std::uint16_t featureId) const override { return featureId == 0x2201 ? 1 : 0; }

    bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                      const std::vector<std::uint8_t>& params) const override
    {
        return G703OnboardPolicy{}.isAllowed(featureId, function, params);
    }

    [[nodiscard]] HidppPointerSensorSource source()
    {
        return HidppPointerSensorSource{HidppNodeLocator{root, "/dev"}, clock, opener()};
    }
};

TEST_F(HidppPointerSensorSourceTest, ReadsTheCapturedBytesWithTheExactRequestSequence)
{
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    ASSERT_TRUE(status->dpiList.has_value());
    ASSERT_TRUE(status->dpiList->valid());
    ASSERT_EQ(status->dpiList->segments().size(), 1U);
    EXPECT_EQ(status->dpiList->segments()[0], (DpiSegment{.minimum = 100, .maximum = 25600, .step = 50}));
    ASSERT_TRUE(status->dpiSetting.has_value());
    EXPECT_EQ(status->dpiSetting->current, 7050);
    EXPECT_EQ(status->dpiSetting->defaultDpi, 800);
    ASSERT_TRUE(status->reportRateSupport.has_value());
    EXPECT_EQ(status->reportRateSupport->periodsMs(), (std::vector<std::uint8_t>{1, 2, 4, 8}));
    EXPECT_EQ(status->currentPeriodMs, 1);
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw12"}));
    EXPECT_EQ(sequence(),
              (std::vector<std::string>{"root 2201", "2201 fn1", "2201 fn2", "root 8060", "8060 fn0", "8060 fn1"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 2) << "índice do dispositivo atrás do receptor";
    }
}

TEST_F(HidppPointerSensorSourceTest, SendsSensorZeroAsTheOnlyDpiParameter)
{
    auto reader = source();

    ASSERT_TRUE(reader.read().has_value());

    for (const auto& frame : io->written()) {
        if (frame[2] == 2) { // 0x2201
            EXPECT_EQ(frame[4], 0x00);
            EXPECT_TRUE(std::all_of(frame.begin() + 4, frame.end(), [](std::uint8_t b) { return b == 0; }));
        }
    }
}

TEST_F(HidppPointerSensorSourceTest, KeepsTheHidrawOpenOnlyWhileReading)
{
    auto reader = source();

    ASSERT_TRUE(reader.read().has_value());

    EXPECT_EQ(openHandles, 0);
    EXPECT_EQ(openedPaths.size(), 1U);
}

TEST_F(HidppPointerSensorSourceTest, AnInvalidDpiListIsNotAnErrorAndKeepsTheRawBytes)
{
    device.responses[{0x2201, 1}] = {0x00, 0x00, 0x64, 0xE0, 0x00, 0x64, 0x00};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    ASSERT_TRUE(status->dpiList.has_value());
    EXPECT_FALSE(status->dpiList->valid());
    ASSERT_GE(status->dpiList->rawBytes().size(), 7U);
    EXPECT_EQ(status->dpiList->rawBytes()[3], 0xE0);
    EXPECT_EQ(status->currentPeriodMs, 1) << "o resto da leitura continua";
}

TEST_F(HidppPointerSensorSourceTest, AbsentFeaturesAreNotInformedWithoutFailingTheRest)
{
    device.features = {0x0001, 0x8060};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    EXPECT_FALSE(status->dpiList.has_value());
    EXPECT_FALSE(status->dpiSetting.has_value());
    EXPECT_EQ(status->currentPeriodMs, 1);
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 2201", "root 8060", "8060 fn0", "8060 fn1"}));
}

TEST_F(HidppPointerSensorSourceTest, ShowsTheDeviceErrorCode)
{
    device.hidpp20Error = 0x02;
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().detail, "o dispositivo respondeu erro HID++ 2.0 (código 0x02)");
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppPointerSensorSourceTest, TimesOutAfterTwoSecondsWithoutResendingAnything)
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

TEST_F(HidppPointerSensorSourceTest, RejectsADpiSettingFromAnotherSensor)
{
    device.responses[{0x2201, 2}] = {0x01, 0x1B, 0x8A, 0x03, 0x20}; // outro sensor
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::MalformedData);
}

TEST_F(HidppPointerSensorSourceTest, FailsWithoutOpeningWhenTheNodeIsNotFound)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw12");
    auto reader = source();

    ASSERT_FALSE(reader.read().has_value());
    EXPECT_TRUE(openedPaths.empty());
}

TEST_F(HidppPointerSensorSourceTest, PropagatesAnOpenFailure)
{
    openFailure = "permissão negada";
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().detail, "permissão negada");
    EXPECT_TRUE(io->written().empty());
}

} // namespace
} // namespace ghub::pointer::infrastructure
