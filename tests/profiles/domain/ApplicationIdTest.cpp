#include "modules/profiles/domain/ApplicationId.hpp"

#include <gtest/gtest.h>

#include <string>

namespace ghub::profiles {
namespace {

using shared_kernel::ErrorCode;

TEST(ApplicationId, NormalizesToLowercaseAfterTrim)
{
    const auto id = ApplicationId::create("  Org.Mozilla.Firefox \t");
    ASSERT_TRUE(id.has_value()) << id.error().detail;
    EXPECT_EQ(id->value(), "org.mozilla.firefox");
}

TEST(ApplicationId, AcceptsOneAnd128Characters)
{
    EXPECT_TRUE(ApplicationId::create("a").has_value());
    EXPECT_TRUE(ApplicationId::create(std::string(128, 'k')).has_value());
}

TEST(ApplicationId, RejectsEmptyAnd129Characters)
{
    for (const std::string& raw : {std::string{}, std::string{"   "}, std::string(129, 'k')}) {
        const auto id = ApplicationId::create(raw);
        ASSERT_FALSE(id.has_value()) << raw.size();
        EXPECT_EQ(id.error().code, ErrorCode::InvalidArgument);
    }
}

TEST(ApplicationId, AcceptsDigitsDotDashAndUnderscore)
{
    const auto id = ApplicationId::create("steam_app_570-x.y");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->value(), "steam_app_570-x.y");
}

TEST(ApplicationId, RejectsSpaceSlashAndNonAscii)
{
    for (const char* raw : {"my app", "usr/bin/x", "café", "a;b", "a\nb"}) {
        const auto id = ApplicationId::create(raw);
        ASSERT_FALSE(id.has_value()) << raw;
        EXPECT_EQ(id.error().code, ErrorCode::InvalidArgument);
    }
}

TEST(ApplicationId, OwnApplicationIsGhubLinux)
{
    EXPECT_EQ(ApplicationId::ownApplication().value(), "ghub-linux");
    EXPECT_EQ(*ApplicationId::create("GHub-Linux"), ApplicationId::ownApplication());
}

} // namespace
} // namespace ghub::profiles
