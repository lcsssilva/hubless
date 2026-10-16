#include "modules/profiles/domain/ProfileLibrary.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

// INC-017 parte 2: seções de hardware do perfil (sidetone, luzes do G733, efeito do G915, DPI do G703).

using ghub::profiles::ApplicationId;
using ghub::profiles::CameraControlSetting;
using ghub::profiles::HardwareSections;
using ghub::profiles::HeadsetLightSetting;
using ghub::profiles::KeyboardEffectSetting;
using ghub::profiles::PointerDpiSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::profiles::ProfileSection;
using ghub::shared_kernel::ErrorCode;

namespace {
ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}
std::vector<CameraControlSetting> controls(std::size_t count)
{
    std::vector<CameraControlSetting> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back({static_cast<std::uint32_t>(i + 1), "c", 0});
    }
    return result;
}
HeadsetLightSetting light(int zone)
{
    return {.zone = zone, .effect = 2, .red = 1, .green = 2, .blue = 3, .periodMs = 5000, .brightness = 80};
}
HardwareSections onlySidetone(int level)
{
    HardwareSections hardware;
    hardware.sidetone = level;
    return hardware;
}
} // namespace

TEST(ProfileHardware, AHardwareSectionAloneIsEnough)
{
    const auto profile = Profile::create(nameOf("a"), {}, std::nullopt, {}, onlySidetone(40));
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->hardware().sidetone, 40);
    EXPECT_TRUE(profile->camera().empty());
}

TEST(ProfileHardware, EmptyHardwareStillRequiresASection)
{
    const auto profile = Profile::create(nameOf("a"), {}, std::nullopt, {}, HardwareSections{});
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}

TEST(ProfileHardware, LegacyOverloadsHaveNoHardware)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt);
    ASSERT_TRUE(profile.has_value());
    EXPECT_TRUE(profile->hardware().empty());
}

TEST(ProfileHardware, HeadsetLightsAreSortedByZoneAndZonesMustBeUnique)
{
    HardwareSections hardware;
    hardware.headsetLights = {light(1), light(0)};
    const auto profile = Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware);
    ASSERT_TRUE(profile.has_value());
    ASSERT_EQ(profile->hardware().headsetLights.size(), 2U);
    EXPECT_EQ(profile->hardware().headsetLights[0].zone, 0);

    hardware.headsetLights = {light(1), light(1)};
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());
    hardware.headsetLights = {light(0), light(1), light(2)};
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());
}

TEST(ProfileHardware, KeyboardEffectNeedsAShortKind)
{
    HardwareSections hardware;
    hardware.keyboardEffect = KeyboardEffectSetting{.kind = "wave", .red = 0, .green = 0, .blue = 0, .brightness = 100};
    EXPECT_TRUE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());
    hardware.keyboardEffect->kind = "";
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());
    hardware.keyboardEffect->kind = std::string(17, 'x');
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());
}

TEST(ProfileHardware, PointerStagesMustBeConsistent)
{
    HardwareSections hardware;
    hardware.pointerDpi =
        PointerDpiSetting{.reportRateHz = 1000, .dpiStages = {800, 1600}, .defaultStage = 1, .dpiShiftStage = 0};
    EXPECT_TRUE(Profile::create(nameOf("a"), {}, std::nullopt, {}, hardware).has_value());

    auto bad = hardware;
    bad.pointerDpi->dpiStages.clear();
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, bad).has_value());
    bad = hardware;
    bad.pointerDpi->dpiStages = {1, 2, 3, 4, 5, 6};
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, bad).has_value());
    bad = hardware;
    bad.pointerDpi->defaultStage = 2;
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, bad).has_value());
    bad = hardware;
    bad.pointerDpi->dpiShiftStage = 5;
    EXPECT_FALSE(Profile::create(nameOf("a"), {}, std::nullopt, {}, bad).has_value());
}

TEST(ProfileHardware, WithApplicationsKeepsEverySection)
{
    const HardwareSections hardware = onlySidetone(10);
    const auto profile = *Profile::create(nameOf("a"), controls(1), std::nullopt, {}, hardware);
    const auto app = *ApplicationId::create("firefox");
    const auto rebuilt = profile.withApplications({app});
    ASSERT_TRUE(rebuilt.has_value());
    EXPECT_EQ(rebuilt->hardware(), hardware);
    EXPECT_EQ(rebuilt->camera(), profile.camera());
    EXPECT_EQ(rebuilt->applications().size(), 1U);
    EXPECT_TRUE(profile.withoutApplications()->applications().empty());
}

TEST(ProfileLibraryHardware, DuplicateCopiesHardwareSections)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(*Profile::create(nameOf("a"), {}, std::nullopt, {}, onlySidetone(55)), false));
    ASSERT_TRUE(library.duplicate(nameOf("a"), nameOf("b")));
    EXPECT_EQ(library.find(nameOf("b"))->hardware().sidetone, 55);
}

TEST(ProfileLibraryHardware, AssignKeepsHardwareSections)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(*Profile::create(nameOf("a"), {}, std::nullopt, {}, onlySidetone(55)), false));
    ASSERT_TRUE(library.assignApplication(nameOf("a"), *ApplicationId::create("firefox")));
    EXPECT_EQ(library.find(nameOf("a"))->hardware().sidetone, 55);
    ASSERT_TRUE(library.unassignApplication(*ApplicationId::create("firefox")));
    EXPECT_EQ(library.find(nameOf("a"))->hardware().sidetone, 55);
}

TEST(ProfileLibraryHardware, EverySectionHasItsOwnPersistentFlag)
{
    ProfileLibrary library;
    using S = ProfileSection;
    for (const S section : {S::Camera, S::Equalizer, S::Sidetone, S::HeadsetLights, S::KeyboardEffect, S::PointerDpi}) {
        EXPECT_FALSE(library.isPersistent(section));
        library.setPersistent(section, true);
        EXPECT_TRUE(library.isPersistent(section));
    }
    library.setPersistent(S::Sidetone, false);
    EXPECT_FALSE(library.isPersistent(S::Sidetone));
    EXPECT_TRUE(library.isPersistent(S::PointerDpi));
}

TEST(ProfileLibraryHardware, PersistentSidetonePropagatesOnlyThatSection)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(*Profile::create(nameOf("a"), {}, std::nullopt, {}, onlySidetone(10)), false));
    ASSERT_TRUE(library.put(*Profile::create(nameOf("b"), {{1, "c", 5}}, std::nullopt, {}, onlySidetone(90)), false));
    library.setPersistent(ProfileSection::Sidetone, true);
    library.propagatePersistent(*library.find(nameOf("a")));
    EXPECT_EQ(library.find(nameOf("b"))->hardware().sidetone, 10);
    EXPECT_EQ(library.find(nameOf("b"))->camera().size(), 1U); // câmera de b intacta
}

TEST(ProfileLibraryHardware, PersistentSectionMissingInSourceIsNotPropagated)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(*Profile::create(nameOf("a"), {{1, "c", 5}}, std::nullopt), false));
    ASSERT_TRUE(library.put(*Profile::create(nameOf("b"), {}, std::nullopt, {}, onlySidetone(90)), false));
    library.setPersistent(ProfileSection::Sidetone, true);
    library.propagatePersistent(*library.find(nameOf("a")));
    EXPECT_EQ(library.find(nameOf("b"))->hardware().sidetone, 90);
}
