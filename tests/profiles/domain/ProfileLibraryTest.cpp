#include "modules/profiles/domain/ProfileLibrary.hpp"

#include <gtest/gtest.h>

#include <string>

using ghub::profiles::CameraControlSetting;
using ghub::profiles::Profile;
using ghub::profiles::ProfileLibrary;
using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;

namespace {
ProfileName nameOf(const std::string& text)
{
    return *ProfileName::create(text);
}
Profile profileOf(const std::string& name, std::int64_t value = 0)
{
    return *Profile::create(nameOf(name), {{1, "c", value}}, std::nullopt);
}
} // namespace

TEST(ProfileLibrary, StartsEmpty)
{
    const ProfileLibrary library;
    EXPECT_TRUE(library.profiles().empty());
    EXPECT_EQ(library.find(nameOf("a")), nullptr);
}

TEST(ProfileLibrary, PreservesInsertionOrder)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(profileOf("b"), false).has_value());
    ASSERT_TRUE(library.put(profileOf("a"), false).has_value());
    ASSERT_EQ(library.profiles().size(), 2U);
    EXPECT_EQ(library.profiles()[0].name().value(), "b");
    EXPECT_EQ(library.profiles()[1].name().value(), "a");
}

TEST(ProfileLibrary, RejectsExistingNameWithoutReplaceAndKeepsLibrary)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(profileOf("a", 1), false).has_value());
    const auto result = library.put(profileOf("a", 2), false);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(result.error().detail.find("já existe"), std::string::npos);
    EXPECT_EQ(library.find(nameOf("a"))->camera()[0].value, 1);
}

TEST(ProfileLibrary, ReplacesInSamePositionWithReplace)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(profileOf("a", 1), false).has_value());
    ASSERT_TRUE(library.put(profileOf("b"), false).has_value());
    ASSERT_TRUE(library.put(profileOf("a", 2), true).has_value());
    ASSERT_EQ(library.profiles().size(), 2U);
    EXPECT_EQ(library.profiles()[0].name().value(), "a");
    EXPECT_EQ(library.profiles()[0].camera()[0].value, 2);
}

TEST(ProfileLibrary, Accepts50AndRejects51stWithoutChange)
{
    ProfileLibrary library;
    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(library.put(profileOf("p" + std::to_string(i)), false).has_value());
    }
    const auto result = library.put(profileOf("extra"), false);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(library.profiles().size(), 50U);
    // Substituir com a biblioteca cheia continua permitido.
    EXPECT_TRUE(library.put(profileOf("p3", 9), true).has_value());
}

TEST(ProfileLibrary, RemovesByName)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(profileOf("a"), false).has_value());
    ASSERT_TRUE(library.put(profileOf("b"), false).has_value());
    ASSERT_TRUE(library.remove(nameOf("a")).has_value());
    ASSERT_EQ(library.profiles().size(), 1U);
    EXPECT_EQ(library.profiles()[0].name().value(), "b");
}

TEST(ProfileLibrary, RemoveOfUnknownNameFails)
{
    ProfileLibrary library;
    ASSERT_TRUE(library.put(profileOf("a"), false).has_value());
    const auto result = library.remove(nameOf("zz"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(library.profiles().size(), 1U);
}

TEST(ProfileLibrary, CreateRejectsDuplicateNames)
{
    std::vector<Profile> profiles{profileOf("a"), profileOf("a", 2)};
    const auto library = ProfileLibrary::create(std::move(profiles));
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error().code, ErrorCode::MalformedData);
}

TEST(ProfileLibrary, CreateRejectsMoreThan50)
{
    std::vector<Profile> profiles;
    for (int i = 0; i < 51; ++i) {
        profiles.push_back(profileOf("p" + std::to_string(i)));
    }
    const auto library = ProfileLibrary::create(std::move(profiles));
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error().code, ErrorCode::MalformedData);
}

TEST(ProfileLibrary, CreateKeepsOrder)
{
    const auto library = ProfileLibrary::create({profileOf("z"), profileOf("a")});
    ASSERT_TRUE(library.has_value());
    EXPECT_EQ(library->profiles()[0].name().value(), "z");
    EXPECT_NE(library->find(nameOf("a")), nullptr);
}

namespace {
using ghub::profiles::ApplicationId;

ApplicationId appOf(const std::string& text)
{
    return *ApplicationId::create(text);
}
Profile profileWith(const std::string& name, std::vector<ApplicationId> applications)
{
    return *Profile::create(nameOf(name), {{1, "c", 0}}, std::nullopt, std::move(applications));
}
ProfileLibrary libraryAB()
{
    ProfileLibrary library;
    EXPECT_TRUE(library.put(profileWith("a", {appOf("firefox")}), false).has_value());
    EXPECT_TRUE(library.put(profileWith("b", {}), false).has_value());
    return library;
}
} // namespace

TEST(ProfileLibrary, AutomaticSwitchingIsOffByDefaultAndCanBeToggled)
{
    ProfileLibrary library;
    EXPECT_FALSE(library.automaticSwitching());
    library.setAutomaticSwitching(true);
    EXPECT_TRUE(library.automaticSwitching());
    library.setAutomaticSwitching(false);
    EXPECT_FALSE(library.automaticSwitching());
}

TEST(ProfileLibrary, ProfileForFindsTheOwnerOfTheApplication)
{
    const ProfileLibrary library = libraryAB();
    const Profile* owner = library.profileFor(appOf("firefox"));
    ASSERT_NE(owner, nullptr);
    EXPECT_EQ(owner->name().value(), "a");
    EXPECT_EQ(library.profileFor(appOf("konsole")), nullptr);
}

TEST(ProfileLibrary, AssignAddsApplicationToProfile)
{
    ProfileLibrary library = libraryAB();
    ASSERT_TRUE(library.assignApplication(nameOf("b"), appOf("konsole")).has_value());
    ASSERT_NE(library.profileFor(appOf("konsole")), nullptr);
    EXPECT_EQ(library.profileFor(appOf("konsole"))->name().value(), "b");
    EXPECT_EQ(library.find(nameOf("b"))->applications().size(), 1U);
}

TEST(ProfileLibrary, AssignToUnknownProfileFails)
{
    ProfileLibrary library = libraryAB();
    const auto result = library.assignApplication(nameOf("zz"), appOf("konsole"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(library.profileFor(appOf("konsole")), nullptr);
}

TEST(ProfileLibrary, AssignToSameProfileTwiceIsNoOp)
{
    ProfileLibrary library = libraryAB();
    ASSERT_TRUE(library.assignApplication(nameOf("a"), appOf("firefox")).has_value());
    EXPECT_EQ(library.find(nameOf("a"))->applications().size(), 1U);
}

TEST(ProfileLibrary, AssignApplicationOfAnotherProfileFailsAndKeepsLibrary)
{
    ProfileLibrary library = libraryAB();
    const auto result = library.assignApplication(nameOf("b"), appOf("firefox"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(result.error().detail.find("já associado ao perfil a"), std::string::npos);
    EXPECT_TRUE(library.find(nameOf("b"))->applications().empty());
    EXPECT_EQ(library.profileFor(appOf("firefox"))->name().value(), "a");
}

TEST(ProfileLibrary, Assign33rdApplicationFails)
{
    ProfileLibrary library = libraryAB();
    for (int i = 0; i < 32; ++i) {
        ASSERT_TRUE(library.assignApplication(nameOf("b"), appOf("app" + std::to_string(i))).has_value());
    }
    const auto result = library.assignApplication(nameOf("b"), appOf("extra"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(library.find(nameOf("b"))->applications().size(), 32U);
}

TEST(ProfileLibrary, UnassignRemovesTheRule)
{
    ProfileLibrary library = libraryAB();
    ASSERT_TRUE(library.unassignApplication(appOf("firefox")).has_value());
    EXPECT_EQ(library.profileFor(appOf("firefox")), nullptr);
    EXPECT_TRUE(library.find(nameOf("a"))->applications().empty());
}

TEST(ProfileLibrary, UnassignOfUnknownApplicationFails)
{
    ProfileLibrary library = libraryAB();
    const auto result = library.unassignApplication(appOf("konsole"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
}

TEST(ProfileLibrary, CreateRejectsApplicationInTwoProfiles)
{
    const auto library =
        ProfileLibrary::create({profileWith("a", {appOf("firefox")}), profileWith("b", {appOf("firefox")})});
    ASSERT_FALSE(library.has_value());
    EXPECT_EQ(library.error().code, ErrorCode::MalformedData);
}

TEST(ProfileLibrary, PutRejectsApplicationOwnedByAnotherProfileAndKeepsLibrary)
{
    ProfileLibrary library = libraryAB();
    const auto result = library.put(profileWith("b", {appOf("firefox")}), true);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(result.error().detail.find("já associado ao perfil a"), std::string::npos);
    EXPECT_TRUE(library.find(nameOf("b"))->applications().empty());
    const auto fresh = library.put(profileWith("c", {appOf("firefox")}), false);
    ASSERT_FALSE(fresh.has_value());
    EXPECT_EQ(library.profiles().size(), 2U);
}

TEST(ProfileLibrary, PutReplacingSameProfileMayKeepItsOwnApplications)
{
    ProfileLibrary library = libraryAB();
    ASSERT_TRUE(library.put(profileWith("a", {appOf("firefox"), appOf("chrome")}), true).has_value());
    EXPECT_EQ(library.find(nameOf("a"))->applications().size(), 2U);
}
