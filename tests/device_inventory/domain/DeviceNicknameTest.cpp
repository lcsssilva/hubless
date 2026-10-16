#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <gtest/gtest.h>

#include <string>

using ghub::device_inventory::DeviceNickname;
using ghub::shared_kernel::ErrorCode;

TEST(DeviceNickname, TrimsAndKeepsTheText)
{
    const auto nickname = DeviceNickname::create("  Teclado da mesa \t");
    ASSERT_TRUE(nickname.has_value());
    EXPECT_EQ(nickname->value(), "Teclado da mesa");
}

TEST(DeviceNickname, AcceptsAccentsAndThirtyTwoCodePoints)
{
    std::string text;
    for (int i = 0; i < 32; ++i) {
        text += "ç";
    }
    EXPECT_TRUE(DeviceNickname::create(text).has_value());
    EXPECT_EQ(DeviceNickname::create(text + "a").error().code, ErrorCode::InvalidArgument);
}

TEST(DeviceNickname, RejectsEmptyOrBlank)
{
    EXPECT_EQ(DeviceNickname::create("").error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(DeviceNickname::create("   ").error().code, ErrorCode::InvalidArgument);
}

TEST(DeviceNickname, RejectsControlCharactersAndInvalidUtf8)
{
    EXPECT_FALSE(DeviceNickname::create("a\nb").has_value());
    EXPECT_FALSE(DeviceNickname::create(std::string("a\0b", 3)).has_value());
    EXPECT_FALSE(DeviceNickname::create("a\x7f").has_value());
    EXPECT_FALSE(DeviceNickname::create("a\xc2\x85").has_value()); // C1 NEL
    EXPECT_FALSE(DeviceNickname::create("a\xff").has_value());
    EXPECT_FALSE(DeviceNickname::create("a\xed\xa0\x80").has_value()); // substituto UTF-16
}
