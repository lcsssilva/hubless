#include "modules/headset/api/HeadsetProfileSettings.hpp"
#include "fakes/FakeClock.hpp"
#include "fakes/FakeExternalProcessProbe.hpp"
#include "fakes/FakeHeadsetSettingsWriter.hpp"

#include <gtest/gtest.h>

#include <vector>

// INC-017 parte 2 (R10): fachada do módulo headset para as seções de perfil (sidetone e luzes por zona).

using ghub::device_access::AccessRegistry;
using ghub::device_access::ExternalOwner;
using ghub::device_access::PhysicalInterfaceId;
using ghub::headset::ChangeHeadsetLighting;
using ghub::headset::ChangeHeadsetSetting;
using ghub::headset::HeadsetLighting;
using ghub::headset::HeadsetLightingWriter;
using ghub::headset::HeadsetProfileSettings;
using ghub::headset::LightZoneValue;
using ghub::shared_kernel::ErrorCode;

namespace {

class RecordingLightingWriter final : public HeadsetLightingWriter {
public:
    [[nodiscard]] ghub::shared_kernel::Result<void> applyLighting(HeadsetLighting lighting) override
    {
        applied.push_back(lighting);
        return result;
    }

    std::vector<HeadsetLighting> applied;
    ghub::shared_kernel::Result<void> result;
};

class HeadsetProfileSettingsTest : public ::testing::Test {
protected:
    HeadsetProfileSettingsTest() { probe.setResult(std::vector<ExternalOwner>{}); }

    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeExternalProcessProbe probe;
    ghub::fakes::FakeHeadsetSettingsWriter settingsWriter;
    RecordingLightingWriter lightingWriter;
    PhysicalInterfaceId interfaceId = PhysicalInterfaceId::create("hidpp:046d:0afe").value();
    ChangeHeadsetSetting changeSetting{settingsWriter, probe, registry, interfaceId};
    ChangeHeadsetLighting changeLighting{lightingWriter, registry, interfaceId};
    HeadsetProfileSettings facade{changeSetting, changeLighting};

    static LightZoneValue breathing(int zone)
    {
        return {.zone = zone, .effect = 2, .red = 0, .green = 255, .blue = 128, .periodMs = 5000, .brightness = 70};
    }
};

} // namespace

TEST_F(HeadsetProfileSettingsTest, AppliesSidetoneAndRemembersIt)
{
    EXPECT_FALSE(facade.lastSidetone().has_value());
    const auto applied = facade.applySidetone(42);
    ASSERT_TRUE(applied.has_value());
    EXPECT_EQ(*applied, 42);
    EXPECT_EQ(settingsWriter.sidetoneValues(), std::vector<int>{42});
    EXPECT_EQ(facade.lastSidetone(), 42);
}

TEST_F(HeadsetProfileSettingsTest, RejectsSidetoneOutOfRangeWithoutWriting)
{
    for (const int level : {-1, 101}) {
        const auto applied = facade.applySidetone(level);
        ASSERT_FALSE(applied.has_value());
        EXPECT_EQ(applied.error().code, ErrorCode::InvalidArgument);
    }
    EXPECT_EQ(settingsWriter.calls(), 0);
    EXPECT_FALSE(facade.lastSidetone().has_value());
}

TEST_F(HeadsetProfileSettingsTest, SidetoneFailureKeepsThePreviousMemory)
{
    ASSERT_TRUE(facade.applySidetone(10).has_value());
    settingsWriter.setResult(ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "headsetcontrol ausente"));
    const auto applied = facade.applySidetone(80);
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::BackendUnavailable);
    EXPECT_EQ(facade.lastSidetone(), 10);
}

TEST_F(HeadsetProfileSettingsTest, AppliesLightZoneAndRemembersItOrderedByZone)
{
    ASSERT_TRUE(facade.applyLight(breathing(1)).has_value());
    ASSERT_TRUE(facade.applyLight(breathing(0)).has_value());
    ASSERT_EQ(lightingWriter.applied.size(), 2U);
    EXPECT_EQ(lightingWriter.applied[0], HeadsetLighting::create(ghub::headset::LightingZone::Upper,
                                                                   ghub::headset::LightingEffect::Breathing,
                                                                   {0, 255, 128}, 5000, 70)
                                             .value());
    const auto last = facade.lastLights();
    ASSERT_EQ(last.size(), 2U);
    EXPECT_EQ(last[0].zone, 0);
    EXPECT_EQ(last[1].zone, 1);
    EXPECT_EQ(last[1].green, 255);
}

TEST_F(HeadsetProfileSettingsTest, RejectsInvalidLightValuesWithoutWriting)
{
    auto bad = breathing(0);
    bad.zone = 2;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing(0);
    bad.effect = 4;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing(0);
    bad.red = 256;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing(0);
    bad.blue = -1;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing(0);
    bad.periodMs = 999;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    bad = breathing(0);
    bad.brightness = 101;
    EXPECT_EQ(facade.applyLight(bad).error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(lightingWriter.applied.empty());
    EXPECT_TRUE(facade.lastLights().empty());
}

TEST_F(HeadsetProfileSettingsTest, LightFailureIsReportedAndNotRemembered)
{
    lightingWriter.result = ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "em uso");
    const auto applied = facade.applyLight(breathing(0));
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(applied.error().code, ErrorCode::DeviceBusy);
    EXPECT_TRUE(facade.lastLights().empty());
}

TEST_F(HeadsetProfileSettingsTest, RememberReplacesTheValueWithoutTouchingTheDevice)
{
    facade.rememberSidetone(30);
    facade.rememberLight(breathing(1));
    facade.rememberLight(breathing(1));
    EXPECT_EQ(facade.lastSidetone(), 30);
    EXPECT_EQ(facade.lastLights().size(), 1U);
    EXPECT_EQ(settingsWriter.calls(), 0);
    EXPECT_TRUE(lightingWriter.applied.empty());
}
