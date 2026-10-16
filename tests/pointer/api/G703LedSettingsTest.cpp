#include "modules/pointer/api/G703LedSettings.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// INC-021 R5: fachada do módulo pointer que grava os dois LEDs fixos mantendo o resto do setor ativo.
namespace ghub::pointer {
namespace {

using Bytes = std::vector<std::uint8_t>;
using shared_kernel::ErrorCode;

class G703LedSettingsTest : public ::testing::Test {
protected:
    G703LedSettingsTest()
    {
        connector.state.sectors[0] = Bytes{fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
        connector.state.sectors[1] = Bytes{fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
        connector.state.sectors[2] = Bytes{fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
    }

    [[nodiscard]] long writes() const
    {
        return std::ranges::count_if(connector.state.calls,
                                     [](const std::string& call) { return call.starts_with("write:"); });
    }

    fakes::FakeClock clock;
    device_access::AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeOnboardMemoryConnector connector;
    fakes::FakeOnboardBackupStore backup;
    G703OnboardSettings onboard{probe, registry, connector, backup};
    G703LedSettings facade{onboard};
};

TEST_F(G703LedSettingsTest, WritesBothLedsFixedAndKeepsRateStagesAndShift)
{
    const auto before = onboard.read().value().settings;
    const auto applied = facade.applyFixed({.red = 10, .green = 200, .blue = 30});
    ASSERT_TRUE(applied.has_value());
    EXPECT_TRUE(applied->wrote);
    const auto after = onboard.read();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->settings.primaryLed(), LedSetting::fixed(10, 200, 30));
    EXPECT_EQ(after->settings.logoLed(), LedSetting::fixed(10, 200, 30));
    EXPECT_EQ(after->settings.reportRateHz(), before.reportRateHz());
    EXPECT_EQ(after->settings.dpiStages(), before.dpiStages());
    EXPECT_EQ(after->settings.defaultStage(), before.defaultStage());
    EXPECT_EQ(after->settings.dpiShiftStage(), before.dpiShiftStage());
    EXPECT_TRUE(after->backupExists);
}

TEST_F(G703LedSettingsTest, SameColorTwiceSendsNothingTheSecondTime)
{
    ASSERT_TRUE(facade.applyFixed({.red = 1, .green = 2, .blue = 3}).has_value());
    const long writesBefore = writes();
    const auto again = facade.applyFixed({.red = 1, .green = 2, .blue = 3});
    ASSERT_TRUE(again.has_value());
    EXPECT_FALSE(again->wrote);
    EXPECT_EQ(writes(), writesBefore);
}

TEST_F(G703LedSettingsTest, ChannelOutOfRangeIsRefusedWithoutWriting)
{
    EXPECT_EQ(facade.applyFixed({.red = 256, .green = 0, .blue = 0}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(facade.applyFixed({.red = 0, .green = -1, .blue = 0}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(writes(), 0);
}

} // namespace
} // namespace ghub::pointer
