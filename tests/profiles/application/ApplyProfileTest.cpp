#include "modules/profiles/application/ApplyProfile.hpp"
#include "fakes/FakeProfilePorts.hpp"

#include <gtest/gtest.h>

using ghub::profiles::ApplyProfile;
using ghub::profiles::EqualizerGains;
using ghub::profiles::Profile;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;
using ghub::test::FakeCameraSettingsPort;
using ghub::test::FakeEqualizerSettingsPort;

namespace {
Profile cameraProfile(std::vector<ghub::profiles::CameraControlSetting> controls)
{
    return *Profile::create(*ProfileName::create("p"), std::move(controls), std::nullopt);
}
} // namespace

TEST(ApplyProfile, AppliesCameraControlsInAscendingIdOrder)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    const ApplyProfile use(camera, eq);
    // Declarados fora de ordem: o auto (0x009a0901) tem de vir antes do absoluto (0x009a0902).
    const auto results = use.applyCamera(
        cameraProfile({{0x009a0902, "Exposição", 300}, {0x0098090c, "WB auto", 1}, {0x009a0901, "Exp auto", 1}}));
    ASSERT_EQ(results.size(), 3U);
    ASSERT_EQ(camera.applied.size(), 3U);
    EXPECT_EQ(camera.applied[0].first, 0x0098090cU);
    EXPECT_EQ(camera.applied[1].first, 0x009a0901U);
    EXPECT_EQ(camera.applied[2].first, 0x009a0902U);
    EXPECT_EQ(results[2].name, "Exposição");
    EXPECT_EQ(results[2].requested, 300);
    ASSERT_TRUE(results[2].outcome.has_value());
    EXPECT_EQ(*results[2].outcome, 300);
}

TEST(ApplyProfile, ReportsReadBackValueFromOwner)
{
    FakeCameraSettingsPort camera;
    camera.applyResults.emplace(1, 128); // dono arredondou
    FakeEqualizerSettingsPort eq;
    const auto results = ApplyProfile(camera, eq).applyCamera(cameraProfile({{1, "Brilho", 130}}));
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0].requested, 130);
    EXPECT_EQ(*results[0].outcome, 128);
}

TEST(ApplyProfile, ContinuesAfterFailureWithoutRetry)
{
    FakeCameraSettingsPort camera;
    camera.applyResults.emplace(2, ghub::shared_kernel::fail(ErrorCode::InvalidArgument, "fora da faixa"));
    FakeEqualizerSettingsPort eq;
    const auto results =
        ApplyProfile(camera, eq).applyCamera(cameraProfile({{1, "a", 10}, {2, "b", 20}, {3, "c", 30}}));
    ASSERT_EQ(results.size(), 3U);
    EXPECT_TRUE(results[0].outcome.has_value());
    ASSERT_FALSE(results[1].outcome.has_value());
    EXPECT_EQ(results[1].outcome.error().code, ErrorCode::InvalidArgument);
    EXPECT_TRUE(results[2].outcome.has_value());
    ASSERT_EQ(camera.applied.size(), 3U); // cada controle uma única vez
    EXPECT_EQ(camera.applied[2].first, 3U);
}

TEST(ApplyProfile, ReportsDeviceBusyPerItem)
{
    FakeCameraSettingsPort camera;
    camera.applyResults.emplace(1, ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "lease ocupado"));
    camera.applyResults.emplace(2, ghub::shared_kernel::fail(ErrorCode::DeviceBusy, "lease ocupado"));
    FakeEqualizerSettingsPort eq;
    const auto results = ApplyProfile(camera, eq).applyCamera(cameraProfile({{1, "a", 1}, {2, "b", 2}}));
    ASSERT_EQ(results.size(), 2U);
    EXPECT_EQ(results[0].outcome.error().code, ErrorCode::DeviceBusy);
    EXPECT_EQ(results[1].outcome.error().code, ErrorCode::DeviceBusy);
}

TEST(ApplyProfile, ProfileWithoutCameraSectionTouchesNoControl)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    const auto profile =
        *Profile::create(*ProfileName::create("p"), {}, *EqualizerGains::create(std::vector<int>(10, 0)));
    EXPECT_TRUE(ApplyProfile(camera, eq).applyCamera(profile).empty());
    EXPECT_TRUE(camera.applied.empty());
}

TEST(ApplyProfile, EqualizerRequestsTheProfileCurve)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    const auto profile =
        *Profile::create(*ProfileName::create("p"), {}, *EqualizerGains::create({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    const auto result = ApplyProfile(camera, eq).applyEqualizer(profile);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_value());
    ASSERT_EQ(eq.requested.size(), 1U);
    EXPECT_EQ(eq.requested[0][0], 1);
    EXPECT_EQ(eq.requested[0][9], 10);
}

TEST(ApplyProfile, EqualizerReturnsOwnerErrorForGainOutOfRange)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    eq.requestResult = ghub::shared_kernel::fail(ErrorCode::InvalidArgument, "ganho 13 fora de -12..12");
    const auto profile =
        *Profile::create(*ProfileName::create("p"), {}, *EqualizerGains::create({13, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    const auto result = ApplyProfile(camera, eq).applyEqualizer(profile);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->has_value());
    EXPECT_EQ((*result).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(eq.requested[0][0], 13); // o domínio do perfil não filtra; o dono recusa
}

TEST(ApplyProfile, EqualizerIsNotTouchedWithoutSection)
{
    FakeCameraSettingsPort camera;
    FakeEqualizerSettingsPort eq;
    const auto result = ApplyProfile(camera, eq).applyEqualizer(cameraProfile({{1, "a", 1}}));
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(eq.requested.empty());
}
