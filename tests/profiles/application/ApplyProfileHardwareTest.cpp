#include "modules/profiles/application/ApplyProfile.hpp"
#include "fakes/FakeProfileHardwarePorts.hpp"
#include "fakes/FakeProfilePorts.hpp"

#include <gtest/gtest.h>

// INC-017 parte 2 (R10/R11): aplicação das seções de hardware, falha isolada por seção.

using ghub::profiles::ApplyProfile;
using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfileSection;
using ghub::profiles::SectionSet;
using ghub::shared_kernel::ErrorCode;
using ghub::test::FakeCameraSettingsPort;
using ghub::test::FakeEqualizerSettingsPort;
using ghub::test::FakeHeadsetSettingsPort;
using ghub::test::FakeKeyboardEffectPort;
using ghub::test::FakePointerSettingsPort;

namespace {

HeadsetLightSetting light(int zone)
{
    return {.zone = zone, .effect = 3, .red = 0, .green = 0, .blue = 0, .periodMs = 5000, .brightness = 100};
}

Profile fullHardwareProfile()
{
    HardwareSections hardware;
    hardware.sidetone = 35;
    hardware.headsetLights = {light(0), light(1)};
    hardware.keyboardEffect = KeyboardEffectSetting{.kind = "wave", .red = 1, .green = 2, .blue = 3, .brightness = 90};
    hardware.pointerDpi =
        PointerDpiSetting{.reportRateHz = 500, .dpiStages = {400, 3200}, .defaultStage = 1, .dpiShiftStage = 0};
    return *Profile::create(*ProfileName::create("p"), {}, std::nullopt, {}, hardware);
}

struct Fixture {
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    FakeHeadsetSettingsPort headset;
    FakeKeyboardEffectPort keyboard;
    FakePointerSettingsPort pointer;
    ApplyProfile use{camera, eq, {.headset = &headset, .keyboard = &keyboard, .pointer = &pointer}};
};

} // namespace

TEST(ApplyProfileHardware, AppliesEverySectionOnItsOwnThreadSplit)
{
    Fixture f;
    const auto profile = fullHardwareProfile();
    const auto worker = f.use.applyHardware(profile, SectionSet{});
    ASSERT_EQ(worker.size(), 4U); // sidetone, 2 zonas, DPI
    for (const auto& item : worker) {
        EXPECT_TRUE(item.outcome.has_value()) << item.label;
    }
    EXPECT_EQ(f.headset.appliedSidetone, std::vector<int>{35});
    ASSERT_EQ(f.headset.appliedLights.size(), 2U);
    EXPECT_EQ(f.headset.appliedLights[0].zone, 0);
    ASSERT_EQ(f.pointer.applied.size(), 1U);
    EXPECT_EQ(f.pointer.applied[0].reportRateHz, 500);
    EXPECT_TRUE(f.keyboard.applied.empty()); // o teclado é da thread dona

    const auto owner = f.use.applyKeyboardEffect(profile, SectionSet{});
    ASSERT_EQ(owner.size(), 1U);
    EXPECT_TRUE(owner[0].outcome.has_value());
    ASSERT_EQ(f.keyboard.applied.size(), 1U);
    EXPECT_EQ(f.keyboard.applied[0].kind, "wave");
}

TEST(ApplyProfileHardware, OneSectionFailingDoesNotStopTheOthers)
{
    Fixture f;
    f.headset.sidetoneResult = ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "em uso por headsetcontrol");
    f.headset.lightResults = {ghub::shared_kernel::fail(ErrorCode::BackendUnavailable, "sem headset")};
    const auto results = f.use.applyHardware(fullHardwareProfile(), SectionSet{});
    ASSERT_EQ(results.size(), 4U);
    EXPECT_FALSE(results[0].outcome.has_value()); // sidetone
    EXPECT_EQ(results[0].outcome.error().code, ErrorCode::DeviceBusy);
    EXPECT_FALSE(results[1].outcome.has_value()); // zona 0
    EXPECT_TRUE(results[2].outcome.has_value());  // zona 1 ainda aplicada
    EXPECT_TRUE(results[3].outcome.has_value());  // DPI ainda aplicado
    EXPECT_EQ(f.pointer.applied.size(), 1U);
}

TEST(ApplyProfileHardware, SkippedPersistentSectionsAreNotApplied)
{
    Fixture f;
    SectionSet skip;
    skip.set(static_cast<std::size_t>(ProfileSection::Sidetone));
    skip.set(static_cast<std::size_t>(ProfileSection::PointerDpi));
    skip.set(static_cast<std::size_t>(ProfileSection::KeyboardEffect));
    const auto results = f.use.applyHardware(fullHardwareProfile(), skip);
    EXPECT_EQ(results.size(), 2U); // só as luzes
    EXPECT_TRUE(f.headset.appliedSidetone.empty());
    EXPECT_TRUE(f.pointer.applied.empty());
    EXPECT_TRUE(f.use.applyKeyboardEffect(fullHardwareProfile(), skip).empty());
    EXPECT_TRUE(f.keyboard.applied.empty());
}

TEST(ApplyProfileHardware, ProfileWithoutHardwareSectionsAppliesNothing)
{
    Fixture f;
    const auto profile = *Profile::create(*ProfileName::create("p"), {{1, "x", 1}}, std::nullopt);
    EXPECT_TRUE(f.use.applyHardware(profile, SectionSet{}).empty());
    EXPECT_TRUE(f.use.applyKeyboardEffect(profile, SectionSet{}).empty());
}

TEST(ApplyProfileHardware, MissingModuleFailsOnlyItsSection)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    FakeHeadsetSettingsPort headset;
    const ApplyProfile use{camera, eq, {.headset = &headset}};
    const auto results = use.applyHardware(fullHardwareProfile(), SectionSet{});
    ASSERT_EQ(results.size(), 4U);
    EXPECT_TRUE(results[0].outcome.has_value());
    EXPECT_FALSE(results[3].outcome.has_value()); // DPI: módulo ausente
    EXPECT_EQ(results[3].outcome.error().code, ErrorCode::BackendUnavailable);
    const auto keyboard = use.applyKeyboardEffect(fullHardwareProfile(), SectionSet{});
    ASSERT_EQ(keyboard.size(), 1U);
    EXPECT_EQ(keyboard[0].outcome.error().code, ErrorCode::BackendUnavailable);
}

TEST(ApplyProfileHardware, PointerAlreadyEqualIsReportedAsSuch)
{
    Fixture f;
    f.pointer.wrote = false;
    const auto results = f.use.applyHardware(fullHardwareProfile(), SectionSet{});
    const auto& dpi = results.back();
    ASSERT_TRUE(dpi.outcome.has_value());
    EXPECT_NE(dpi.outcome->find("já"), std::string::npos);
}
