#include "modules/headset/infrastructure/HidppHeadsetStatusSource.hpp"
#include "fakes/StatusReadHarness.hpp"
#include "modules/headset/infrastructure/G733StatusPolicy.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace ghub::headset::infrastructure {
namespace {

using platform::hidpp::HidppNodeLocator;
using shared_kernel::ErrorCode;

class HidppHeadsetStatusSourceTest : public fakes::StatusReadFixture {
protected:
    void SetUp() override
    {
        StatusReadFixture::SetUp();
        // Bytes das capturas C-00/C-01/C-03 do G733 (INC-015 parte 2): 0x8310 fn0 -> 0a 0c 01; 0x8300 fn0 zerada.
        device.features = {0x0001, 0x8300, 0x8310};
        device.responses[{0x8310, 0}] = {0x0A, 0x0C, 0x01};
        device.responses[{0x8300, 0}] = {};
    }

    bool frameAllowed(std::uint16_t featureId, std::uint8_t function,
                      const std::vector<std::uint8_t>& params) const override
    {
        return G733StatusPolicy{}.isAllowed(featureId, function, params);
    }

    [[nodiscard]] HidppHeadsetStatusSource source()
    {
        return HidppHeadsetStatusSource{HidppNodeLocator{root, "/dev"}, clock, opener()};
    }
};

TEST_F(HidppHeadsetStatusSourceTest, ReadsTheCapturedBytesWithTheExactRequestSequence)
{
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    ASSERT_TRUE(status->equalizer.has_value());
    EXPECT_EQ(status->equalizer->bandCount, 10);
    EXPECT_EQ(status->equalizer->unexplained, (std::array<std::uint8_t, 2>{0x0C, 0x01}));
    ASSERT_TRUE(status->uninterpreted.has_value());
    EXPECT_EQ(status->uninterpreted->bytes, (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
    EXPECT_EQ(openedPaths, (std::vector<std::filesystem::path>{"/dev/hidraw3"}));
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8310", "8310 fn0", "root 8300", "8300 fn0"}));
    for (const auto& frame : io->written()) {
        EXPECT_EQ(frame[1], 0xFF) << "índice de dispositivo do G733";
    }
}

TEST_F(HidppHeadsetStatusSourceTest, KeepsTheUninterpretedBytesRaw)
{
    device.responses[{0x8300, 0}] = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    EXPECT_EQ(status->uninterpreted->bytes, (std::array<std::uint8_t, 4>{1, 2, 3, 4}));
}

TEST_F(HidppHeadsetStatusSourceTest, NeverSendsTheSidetoneWrite)
{
    auto reader = source();

    ASSERT_TRUE(reader.read().has_value());

    for (const auto& frame : io->written()) {
        if (frame[2] != 0) {
            EXPECT_EQ(frame[3] >> 4, 0) << "só fn0 em feature que não é a Root";
        }
    }
}

TEST_F(HidppHeadsetStatusSourceTest, KeepsTheHidrawOpenOnlyWhileReading)
{
    auto reader = source();

    ASSERT_TRUE(reader.read().has_value());

    EXPECT_EQ(openHandles, 0);
    EXPECT_EQ(openedPaths.size(), 1U);
}

TEST_F(HidppHeadsetStatusSourceTest, AbsentFeatureIsNotInformedWithoutFailingTheRest)
{
    device.features = {0x0001, 0x8310};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_TRUE(status.has_value()) << status.error().detail;
    EXPECT_TRUE(status->equalizer.has_value());
    EXPECT_FALSE(status->uninterpreted.has_value());
    EXPECT_EQ(sequence(), (std::vector<std::string>{"root 8310", "8310 fn0", "root 8300"}));
}

TEST_F(HidppHeadsetStatusSourceTest, ErrorFiveMeansTheHeadsetIsOffOrOutOfRange)
{
    device.hidpp20Error = 0x05;
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(status.error().detail, "headset desligado ou fora de alcance");
    EXPECT_EQ(openHandles, 0);
}

TEST_F(HidppHeadsetStatusSourceTest, OtherDeviceErrorsShowTheirCode)
{
    device.hidpp20Error = 0x07;
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().detail, "o dispositivo respondeu erro HID++ 2.0 (código 0x07)");
}

TEST_F(HidppHeadsetStatusSourceTest, TimesOutAfterTwoSecondsWithoutResendingAnything)
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

TEST_F(HidppHeadsetStatusSourceTest, RejectsAZeroBandEqualizer)
{
    device.responses[{0x8310, 0}] = {0x00, 0x0C, 0x01};
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, ErrorCode::MalformedData);
}

TEST_F(HidppHeadsetStatusSourceTest, FailsWithoutOpeningWhenTheNodeIsNotFound)
{
    std::filesystem::remove_all(root / "class" / "hidraw" / "hidraw3");
    auto reader = source();

    ASSERT_FALSE(reader.read().has_value());
    EXPECT_TRUE(openedPaths.empty());
}

TEST_F(HidppHeadsetStatusSourceTest, PropagatesAnOpenFailure)
{
    openFailure = "permissão negada";
    auto reader = source();

    const auto status = reader.read();

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().detail, "permissão negada");
    EXPECT_TRUE(io->written().empty());
}

} // namespace
} // namespace ghub::headset::infrastructure
