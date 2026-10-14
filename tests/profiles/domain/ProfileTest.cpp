#include "modules/profiles/domain/Profile.hpp"

#include <gtest/gtest.h>

#include <string>

using ghub::profiles::CameraControlSetting;
using ghub::profiles::EqualizerGains;
using ghub::profiles::Profile;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;

namespace {
ProfileName nameOf(const char* text)
{
    return *ProfileName::create(text);
}
EqualizerGains flatGains()
{
    return *EqualizerGains::create(std::vector<int>(10, 0));
}
std::vector<CameraControlSetting> controls(std::size_t count)
{
    std::vector<CameraControlSetting> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back({static_cast<std::uint32_t>(i + 1), "c", 0});
    }
    return result;
}
} // namespace

TEST(Profile, RejectsProfileWithoutSections)
{
    const auto profile = Profile::create(nameOf("a"), {}, std::nullopt);
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}

TEST(Profile, AcceptsCameraOnly)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt);
    ASSERT_TRUE(profile.has_value());
    EXPECT_FALSE(profile->equalizer().has_value());
    EXPECT_EQ(profile->name().value(), "a");
}

TEST(Profile, AcceptsEqualizerOnly)
{
    const auto profile = Profile::create(nameOf("a"), {}, flatGains());
    ASSERT_TRUE(profile.has_value());
    EXPECT_TRUE(profile->camera().empty());
    EXPECT_TRUE(profile->equalizer().has_value());
}

TEST(Profile, Accepts64CameraControls)
{
    EXPECT_TRUE(Profile::create(nameOf("a"), controls(64), std::nullopt).has_value());
}

TEST(Profile, Rejects65CameraControls)
{
    const auto profile = Profile::create(nameOf("a"), controls(65), std::nullopt);
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}

TEST(Profile, RejectsRepeatedControlId)
{
    const auto profile = Profile::create(nameOf("a"), {{5, "x", 1}, {7, "y", 2}, {5, "z", 3}}, std::nullopt);
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}

TEST(Profile, ControlNameLimitIs64Bytes)
{
    EXPECT_TRUE(Profile::create(nameOf("a"), {{1, std::string(64, 'n'), 1}}, std::nullopt).has_value());
    const auto tooLong = Profile::create(nameOf("a"), {{1, std::string(65, 'n'), 1}}, std::nullopt);
    ASSERT_FALSE(tooLong.has_value());
    EXPECT_EQ(tooLong.error().code, ErrorCode::InvalidArgument);
}

TEST(Profile, KeepsCameraSortedByAscendingId)
{
    const auto profile =
        Profile::create(nameOf("a"), {{0x009a0902, "b", 1}, {0x0098090c, "a", 2}, {0x009a0901, "c", 3}}, std::nullopt);
    ASSERT_TRUE(profile.has_value());
    ASSERT_EQ(profile->camera().size(), 3U);
    EXPECT_EQ(profile->camera()[0].id, 0x0098090cU);
    EXPECT_EQ(profile->camera()[1].id, 0x009a0901U);
    EXPECT_EQ(profile->camera()[2].id, 0x009a0902U);
}

TEST(Profile, KeepsControlValuesUnvalidated)
{
    const auto profile = Profile::create(nameOf("a"), {{1, "x", -9'000'000'000}}, std::nullopt);
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->camera()[0].value, -9'000'000'000);
}

namespace {
ghub::profiles::ApplicationId appOf(const std::string& text)
{
    return *ghub::profiles::ApplicationId::create(text);
}
std::vector<ghub::profiles::ApplicationId> apps(std::size_t count)
{
    std::vector<ghub::profiles::ApplicationId> result;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(appOf("app" + std::to_string(i)));
    }
    return result;
}
} // namespace

TEST(Profile, HasNoApplicationsByDefault)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt);
    ASSERT_TRUE(profile.has_value());
    EXPECT_TRUE(profile->applications().empty());
}

TEST(Profile, KeepsApplicationsSortedAndAcceptsExactly32)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt, {appOf("konsole"), appOf("firefox")});
    ASSERT_TRUE(profile.has_value());
    ASSERT_EQ(profile->applications().size(), 2U);
    EXPECT_EQ(profile->applications()[0].value(), "firefox");
    EXPECT_EQ(profile->applications()[1].value(), "konsole");
    EXPECT_TRUE(Profile::create(nameOf("a"), controls(1), std::nullopt, apps(32)).has_value());
}

TEST(Profile, Rejects33Applications)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt, apps(33));
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}

TEST(Profile, RejectsRepeatedApplication)
{
    const auto profile = Profile::create(nameOf("a"), controls(1), std::nullopt, {appOf("x"), appOf("y"), appOf("X")});
    ASSERT_FALSE(profile.has_value());
    EXPECT_EQ(profile.error().code, ErrorCode::InvalidArgument);
}
