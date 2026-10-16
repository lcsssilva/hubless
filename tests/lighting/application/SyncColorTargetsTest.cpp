#include "modules/lighting/application/SyncColorTargets.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeHeadsetSettingsWriter.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <vector>

// INC-021 R5: adaptadores dos alvos da sincronização sobre as api/ dos módulos donos.
namespace ghub::lighting::application {
namespace {

using domain::RgbColor;
using shared_kernel::ErrorCode;

class RecordingKeyboardApplier final : public KeyboardEffectApplier {
public:
    [[nodiscard]] shared_kernel::Result<void> applyEffect(const KeyboardEffectValue& value) override
    {
        applied.push_back(value);
        return result;
    }
    std::vector<KeyboardEffectValue> applied;
    shared_kernel::Result<void> result;
};

TEST(KeyboardEffectTarget, SendsAStaticEffectWithTheColorAndReportsLiveness)
{
    RecordingKeyboardApplier applier;
    KeyboardEffectSettings settings{applier};
    bool live = false;
    KeyboardEffectTarget target{settings, [&live] { return live; }};
    EXPECT_FALSE(target.ready());
    live = true;
    EXPECT_TRUE(target.ready());
    ASSERT_TRUE(target.applyFixed(RgbColor{1, 2, 3}).has_value());
    ASSERT_EQ(applier.applied.size(), 1U);
    EXPECT_EQ(applier.applied.front().kind, "static");
    EXPECT_EQ(applier.applied.front().red, 1);
    EXPECT_EQ(applier.applied.front().green, 2);
    EXPECT_EQ(applier.applied.front().blue, 3);
}

class RecordingLightingWriter final : public headset::HeadsetLightingWriter {
public:
    [[nodiscard]] shared_kernel::Result<void> applyLighting(headset::HeadsetLighting lighting) override
    {
        applied.push_back(lighting);
        if (failOn.has_value() && lighting.zone() == *failOn) {
            return shared_kernel::fail(ErrorCode::DeviceBusy, "zona ocupada");
        }
        return {};
    }
    std::vector<headset::HeadsetLighting> applied;
    std::optional<headset::LightingZone> failOn;
};

class HeadsetFixedTargetTest : public ::testing::Test {
protected:
    HeadsetFixedTargetTest() { probe.setResult(std::vector<device_access::ExternalOwner>{}); }

    fakes::FakeClock clock;
    device_access::AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeHeadsetSettingsWriter settingsWriter;
    RecordingLightingWriter lightingWriter;
    device_access::PhysicalInterfaceId interfaceId = device_access::PhysicalInterfaceId::create("hidpp:046d:0afe").value();
    headset::ChangeHeadsetSetting changeSetting{settingsWriter, probe, registry, interfaceId};
    headset::ChangeHeadsetLighting changeLighting{lightingWriter, registry, interfaceId};
    headset::HeadsetProfileSettings facade{changeSetting, changeLighting};
    HeadsetFixedTarget target{facade};
};

TEST_F(HeadsetFixedTargetTest, AppliesFixedToBothZonesWithTheColor)
{
    ASSERT_TRUE(target.applyFixed(RgbColor{9, 8, 7}).has_value());
    ASSERT_EQ(lightingWriter.applied.size(), 2U);
    EXPECT_EQ(lightingWriter.applied[0].payload(), (std::vector<std::uint8_t>{0, 1, 9, 8, 7, 2}));
    EXPECT_EQ(lightingWriter.applied[1].payload(), (std::vector<std::uint8_t>{1, 1, 9, 8, 7, 2}));
}

TEST_F(HeadsetFixedTargetTest, AFailingZoneIsReportedButTheOtherZoneIsStillTried)
{
    lightingWriter.failOn = headset::LightingZone::Lower;
    const auto result = target.applyFixed(RgbColor{9, 8, 7});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(lightingWriter.applied.size(), 2U);
}

TEST(MouseLedTarget, WritesBothLedsFixedOnTheActiveProfile)
{
    using Bytes = std::vector<std::uint8_t>;
    fakes::FakeClock clock;
    device_access::AccessRegistry registry{clock};
    fakes::FakeExternalProcessProbe probe;
    fakes::FakeOnboardMemoryConnector connector;
    fakes::FakeOnboardBackupStore backup;
    connector.state.sectors[0] = Bytes{fixtures::kDirectory0000.begin(), fixtures::kDirectory0000.end()};
    connector.state.sectors[1] = Bytes{fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
    connector.state.sectors[2] = Bytes{fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
    pointer::G703OnboardSettings onboard{probe, registry, connector, backup};
    const pointer::G703LedSettings facade{onboard};
    MouseLedTarget target{facade};
    ASSERT_TRUE(target.applyFixed(RgbColor{4, 5, 6}).has_value());
    const auto after = onboard.read();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->settings.primaryLed(), pointer::LedSetting::fixed(4, 5, 6));
    EXPECT_EQ(after->settings.logoLed(), pointer::LedSetting::fixed(4, 5, 6));
}

} // namespace
} // namespace ghub::lighting::application
