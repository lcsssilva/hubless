#include "modules/lighting/domain/G915KeyCatalog.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>

using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::KeyId;

namespace {

// Zonas do OpenRGB release_1.0 (LogitechG915Controller.h:22-27 e :39-42).
constexpr std::uint8_t kZoneKeyboard = 0x01;
constexpr std::uint8_t kZoneMedia = 0x02;
constexpr std::uint8_t kZoneGKeys = 0x03;
constexpr std::uint8_t kZoneModifiers = 0x04;
constexpr std::uint8_t kZoneLogo = 0x10;
constexpr std::uint8_t kZoneIndicators = 0x40;

KeyId keyNamed(const char* name)
{
    const auto id = G915KeyCatalog::findByName(name);
    EXPECT_TRUE(id.has_value()) << name;
    return id.value(); // NOLINT(bugprone-unchecked-optional-access) -- vazio lança e o teste falha
}

} // namespace

// led_names[] de RGBController_LogitechG915.cpp@release_1.0 tem 117 entradas (full_led_count = 117).
TEST(G915KeyCatalogTest, HasTheFullLayoutKeyCount)
{
    EXPECT_EQ(G915KeyCatalog::keys().size(), 117U);
    EXPECT_EQ(G915KeyCatalog::kFullLayoutKeyCount, 117U);
}

// tkl_led_count = 95; a primeira tecla fora do TKL é Num Lock (posição 95).
TEST(G915KeyCatalogTest, TklLayoutIsThePrefixBeforeNumLock)
{
    EXPECT_EQ(G915KeyCatalog::kTklKeyCount, 95U);
    EXPECT_TRUE(G915KeyCatalog::isInTklLayout(keyNamed("Key: Brightness")));
    EXPECT_FALSE(G915KeyCatalog::isInTklLayout(keyNamed("Key: Num Lock")));
    EXPECT_EQ(keyNamed("Key: Num Lock").position(), 95U);
}

TEST(G915KeyCatalogTest, NamesAreUnique)
{
    std::set<std::string> names;
    for (const auto& key : G915KeyCatalog::keys()) {
        names.emplace(key.name);
    }
    EXPECT_EQ(names.size(), G915KeyCatalog::keys().size());
}

TEST(G915KeyCatalogTest, ZoneAndIndexPairsAreUnique)
{
    std::set<std::pair<int, int>> addresses;
    for (const auto& key : G915KeyCatalog::keys()) {
        addresses.emplace(key.zone, key.index);
    }
    EXPECT_EQ(addresses.size(), G915KeyCatalog::keys().size());
}

TEST(G915KeyCatalogTest, KeysCarryTheirZoneAndIndex)
{
    const auto a = G915KeyCatalog::definition(keyNamed("Key: A"));
    EXPECT_EQ(a.zone, kZoneKeyboard);
    EXPECT_EQ(a.index, 0x04);

    const auto leftControl = G915KeyCatalog::definition(keyNamed("Key: Left Control"));
    EXPECT_EQ(leftControl.zone, kZoneModifiers);
    EXPECT_EQ(leftControl.index, 0xE0);

    const auto mute = G915KeyCatalog::definition(keyNamed("Key: Media Mute"));
    EXPECT_EQ(mute.zone, kZoneMedia);
    EXPECT_EQ(mute.index, 0x9C);

    const auto logo = G915KeyCatalog::definition(keyNamed("Logo"));
    EXPECT_EQ(logo.zone, kZoneLogo);
    EXPECT_EQ(logo.index, 0x01);

    const auto brightness = G915KeyCatalog::definition(keyNamed("Key: Brightness"));
    EXPECT_EQ(brightness.zone, kZoneIndicators);
    EXPECT_EQ(brightness.index, 0x99);

    const auto g5 = G915KeyCatalog::definition(keyNamed("Key: G5"));
    EXPECT_EQ(g5.zone, kZoneGKeys);
    EXPECT_EQ(g5.index, 0x05);
}

TEST(G915KeyCatalogTest, ResolvesBackslashNamesFromTheKeyNameMacros)
{
    EXPECT_TRUE(G915KeyCatalog::findByName(R"(Key: \ (ANSI))").has_value());
    EXPECT_TRUE(G915KeyCatalog::findByName(R"(Key: \ (ISO))").has_value());
}

TEST(G915KeyCatalogTest, UnknownNameOrAddressIsNotFound)
{
    EXPECT_FALSE(G915KeyCatalog::findByName("Key: Nonexistent").has_value());
    EXPECT_FALSE(G915KeyCatalog::findByAddress(kZoneKeyboard, 0xFF).has_value());
}

TEST(G915KeyCatalogTest, FindsAKeyByZoneAndIndex)
{
    const auto id = G915KeyCatalog::findByAddress(kZoneGKeys, 0x01);

    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id.value(), keyNamed("Key: G1")); // NOLINT(bugprone-unchecked-optional-access) -- ASSERT acima
}

TEST(G915KeyCatalogTest, KeyIdsDifferPerKeyAndOrderByPosition)
{
    EXPECT_NE(keyNamed("Key: A"), keyNamed("Key: B"));
    EXPECT_LT(keyNamed("Key: A"), keyNamed("Key: B"));
}
