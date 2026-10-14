#include "modules/profiles/domain/ProfileName.hpp"

#include <gtest/gtest.h>

#include <string>

using ghub::profiles::ProfileName;
using ghub::shared_kernel::ErrorCode;

namespace {
void expectInvalid(const std::string& raw)
{
    const auto name = ProfileName::create(raw);
    ASSERT_FALSE(name.has_value()) << "aceitou: " << raw;
    EXPECT_EQ(name.error().code, ErrorCode::InvalidArgument);
}
} // namespace

TEST(ProfileName, RejectsEmpty)
{
    expectInvalid("");
}

TEST(ProfileName, RejectsOnlySpaces)
{
    expectInvalid("   \t ");
}

TEST(ProfileName, AcceptsOneCharacter)
{
    const auto name = ProfileName::create("a");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name->value(), "a");
}

TEST(ProfileName, TrimsAsciiSpaces)
{
    const auto name = ProfileName::create("  Jogo FPS \t");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name->value(), "Jogo FPS");
}

TEST(ProfileName, AcceptsExactly64Characters)
{
    EXPECT_TRUE(ProfileName::create(std::string(64, 'x')).has_value());
}

TEST(ProfileName, Rejects65Characters)
{
    expectInvalid(std::string(65, 'x'));
}

TEST(ProfileName, CountsAccentedCharacterAsOne)
{
    // "é" ocupa 2 bytes em UTF-8: 64 deles são 128 bytes mas 64 caracteres.
    std::string sixtyFour;
    for (int i = 0; i < 64; ++i) {
        sixtyFour += "\xC3\xA9";
    }
    EXPECT_TRUE(ProfileName::create(sixtyFour).has_value());
    expectInvalid(sixtyFour + "\xC3\xA9");
}

TEST(ProfileName, CountsFourByteCharacterAsOne)
{
    std::string sixtyFour;
    for (int i = 0; i < 64; ++i) {
        sixtyFour += "\xF0\x9F\x8E\xAE";
    }
    EXPECT_TRUE(ProfileName::create(sixtyFour).has_value());
}

TEST(ProfileName, RejectsControlCharacters)
{
    expectInvalid("a\nb");
    expectInvalid(std::string("a\0b", 3));
    expectInvalid("a\x7F");
    expectInvalid("a\x1F"
                  "b");
}

TEST(ProfileName, RejectsInvalidUtf8)
{
    expectInvalid("\xFF");
    expectInvalid("a\xC3");            // sequência truncada
    expectInvalid("\xC0\xAF");         // codificação longa demais
    expectInvalid("\xED\xA0\x80");     // substituto UTF-16
    expectInvalid("\xF4\x90\x80\x80"); // acima de U+10FFFF
    expectInvalid("\x80");             // byte de continuação solto
}

TEST(ProfileName, ComparesByExactValue)
{
    EXPECT_EQ(*ProfileName::create(" a "), *ProfileName::create("a"));
    EXPECT_FALSE(*ProfileName::create("a") == *ProfileName::create("A"));
}
