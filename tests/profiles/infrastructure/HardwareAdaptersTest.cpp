#include "modules/profiles/infrastructure/HardwareSettingsAdapters.hpp"

#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeHeadsetSettingsWriter.hpp"
#include "fakes/FakeOnboardMemory.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <vector>

// INC-017 parte 2 (R10): os adaptadores do profiles só traduzem tipos e chamam a api/ do módulo dono.

using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::HeadsetSettingsAdapter;
using ghub::profiles::KeyboardEffectAdapter;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::PointerSettingsAdapter;
using ghub::shared_kernel::ErrorCode;

namespace {

class RecordingLightingWriter final : public ghub::headset::HeadsetLightingWriter {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyLighting(ghub::headset::HeadsetLighting lighting) override
    {
        applied.push_back(lighting);
        return {};
    }
    std::vector<ghub::headset::HeadsetLighting> applied;
};

class HeadsetAdapterTest : public ::testing::Test {
protected:
    HeadsetAdapterTest() { probe.setResult(std::vector<ghub::device_access::ExternalOwner>{}); }

    ghub::fakes::FakeClock clock;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeHeadsetSettingsWriter settingsWriter;
    RecordingLightingWriter lightingWriter;
    ghub::device_access::PhysicalInterfaceId interfaceId =
        ghub::device_access::PhysicalInterfaceId::create("hidpp:046d:0afe").value();
    ghub::headset::ChangeHeadsetSetting changeSetting{settingsWriter, probe, registry, interfaceId};
    ghub::headset::ChangeHeadsetLighting changeLighting{lightingWriter, registry, interfaceId};
    ghub::headset::HeadsetProfileSettings facade{changeSetting, changeLighting};
    HeadsetSettingsAdapter adapter{facade};
};

} // namespace

TEST_F(HeadsetAdapterTest, SidetoneGoesThroughTheOwnerFacadeAndIsRemembered)
{
    EXPECT_FALSE(adapter.lastSidetone().has_value());
    const auto applied = adapter.applySidetone(55);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(*applied, 55);
    EXPECT_EQ(settingsWriter.sidetoneValues(), std::vector<int>{55});
    EXPECT_EQ(adapter.lastSidetone(), 55);
    EXPECT_EQ(adapter.applySidetone(200).error().code, ErrorCode::InvalidArgument);
}

TEST_F(HeadsetAdapterTest, LightsGoThroughTheOwnerFacadeAndTypesRoundTrip)
{
    const HeadsetLightSetting light{.zone = 1, .effect = 2, .red = 10, .green = 20, .blue = 30, .periodMs = 3000,
                                    .brightness = 60};
    ASSERT_TRUE(adapter.applyLight(light).has_value());
    EXPECT_EQ(lightingWriter.applied.size(), 1U);
    ASSERT_EQ(adapter.lastLights().size(), 1U);
    EXPECT_EQ(adapter.lastLights()[0], light);
    auto bad = light;
    bad.zone = 5;
    EXPECT_EQ(adapter.applyLight(bad).error().code, ErrorCode::InvalidArgument);
}

namespace {

class RecordingApplier final : public ghub::lighting::KeyboardEffectApplier {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void>
    applyEffect(const ghub::lighting::KeyboardEffectValue& value) override
    {
        requested.push_back(value);
        return {};
    }
    std::vector<ghub::lighting::KeyboardEffectValue> requested;
};

} // namespace

TEST(KeyboardEffectAdapterTest, ForwardsToTheLightingFacadeAndKeepsTheLastEffect)
{
    RecordingApplier applier;
    ghub::lighting::KeyboardEffectSettings facade{applier};
    KeyboardEffectAdapter adapter{facade};
    EXPECT_FALSE(adapter.last().has_value());
    const KeyboardEffectSetting effect{.kind = "wave", .red = 1, .green = 2, .blue = 3, .brightness = 90};
    ASSERT_TRUE(adapter.apply(effect).has_value());
    ASSERT_EQ(applier.requested.size(), 1U);
    EXPECT_EQ(applier.requested[0].kind, "wave");
    EXPECT_EQ(applier.requested[0].brightness, 90);
    EXPECT_EQ(adapter.last(), effect);
    EXPECT_EQ(adapter.apply({.kind = "nope", .red = 0, .green = 0, .blue = 0, .brightness = 50}).error().code,
              ErrorCode::InvalidArgument);
}

namespace {

class PointerAdapterTest : public ::testing::Test {
protected:
    PointerAdapterTest()
    {
        using Bytes = std::vector<std::uint8_t>;
        connector.state.sectors[0] =
            Bytes{ghub::fixtures::kDirectory0000.begin(), ghub::fixtures::kDirectory0000.end()};
        connector.state.sectors[1] =
            Bytes{ghub::fixtures::kSector0001.begin(), ghub::fixtures::kSector0001.end()};
        connector.state.sectors[2] =
            Bytes{ghub::fixtures::kSector0002.begin(), ghub::fixtures::kSector0002.end()};
    }

    ghub::fakes::FakeClock clock;
    ghub::device_access::AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeOnboardMemoryConnector connector;
    ghub::fakes::FakeOnboardBackupStore backup;
    ghub::pointer::G703OnboardSettings onboard{probe, registry, connector, backup};
    ghub::pointer::G703DpiSettings facade{onboard};
    PointerSettingsAdapter adapter{facade};
};

} // namespace

TEST_F(PointerAdapterTest, ReadsTheActiveProfileAndWritesThroughTheSafePath)
{
    const auto current = adapter.readCurrent();
    ASSERT_TRUE(current.has_value());
    EXPECT_EQ(current->reportRateHz, 1000);
    EXPECT_EQ(current->dpiStages, std::vector<int>{7050});

    const PointerDpiSetting wanted{.reportRateHz = 500, .dpiStages = {400, 1600}, .defaultStage = 1, .dpiShiftStage = 0};
    const auto applied = adapter.apply(wanted);
    ASSERT_TRUE(applied.has_value()) << applied.error().detail;
    EXPECT_EQ(applied->setting, wanted);
    EXPECT_TRUE(applied->wrote);
    EXPECT_EQ(backup.saveCalls, 1); // a cópia automática do dono
}

TEST_F(PointerAdapterTest, OwnerRefusalIsPropagated)
{
    probe.setResult(std::vector<ghub::device_access::ExternalOwner>{ghub::device_access::ExternalOwner::Ratbagd});
    const auto applied = adapter.apply({.reportRateHz = 500, .dpiStages = {800}, .defaultStage = 0, .dpiShiftStage = 0});
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().detail, "em uso por ratbagd");
}
