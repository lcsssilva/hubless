#include "modules/lighting/domain/GameModeKeys.hpp"

#include "modules/lighting/domain/G915KeyCatalog.hpp"
#include "modules/lighting/domain/OnboardSlot.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using ghub::lighting::domain::G915KeyCatalog;
using ghub::lighting::domain::GameModeKeys;
using ghub::lighting::domain::KeyId;
using ghub::lighting::domain::OnboardSlot;
using ghub::shared_kernel::ErrorCode;

KeyId key(const char* name)
{
    return *G915KeyCatalog::findByName(name); // NOLINT(bugprone-unchecked-optional-access)
}

} // namespace

TEST(GameModeKeys, SuggestedDefaultIsTheWindowsKeysAndMenu)
{
    const auto keys = GameModeKeys::suggested();

    // Left Windows 0xE3, Right Windows 0xE7, Menu 0x65 (códigos de uso HID do catálogo).
    EXPECT_EQ(keys.hidCodes(), (std::vector<std::uint8_t>{0xE3, 0xE7, 0x65}));
}

TEST(GameModeKeys, ConvertsCatalogKeysToHidCodes)
{
    const auto keys = GameModeKeys::create(std::vector<KeyId>{key("Key: F1")});

    ASSERT_TRUE(keys.has_value());
    EXPECT_EQ(keys->hidCodes(), (std::vector<std::uint8_t>{0x3A})); // C-04: F1 = 0x3a
}

TEST(GameModeKeys, EmptyListMeansReenableEveryKey)
{
    const auto keys = GameModeKeys::create({});

    ASSERT_TRUE(keys.has_value());
    EXPECT_TRUE(keys->hidCodes().empty());
}

TEST(GameModeKeys, DuplicatesAreSentOnce)
{
    const auto keys = GameModeKeys::create(std::vector<KeyId>{key("Key: F1"), key("Key: F1")});

    ASSERT_TRUE(keys.has_value());
    EXPECT_EQ(keys->hidCodes().size(), 1U);
}

TEST(GameModeKeys, RefusesKeysThatAreNotKeyboardUsages)
{
    for (const char* name : {"Key: Media Mute", "Logo", "Key: G1", "Key: Brightness"}) {
        const auto keys = GameModeKeys::create(std::vector<KeyId>{key(name)});
        ASSERT_FALSE(keys.has_value()) << name;
        EXPECT_EQ(keys.error().code, ErrorCode::InvalidArgument) << name;
    }
}

TEST(GameModeKeys, RefusesMoreThanSixteenKeys)
{
    std::vector<KeyId> many;
    for (const char* name : {"Key: A", "Key: B", "Key: C", "Key: D", "Key: E", "Key: F", "Key: G", "Key: H", "Key: I",
                             "Key: J", "Key: K", "Key: L", "Key: M", "Key: N", "Key: O", "Key: P"}) {
        many.push_back(key(name));
    }
    ASSERT_TRUE(GameModeKeys::create(many).has_value()) << "16 teclas cabem";

    many.push_back(key("Key: Q"));
    const auto keys = GameModeKeys::create(many);

    ASSERT_FALSE(keys.has_value());
    EXPECT_EQ(keys.error().code, ErrorCode::InvalidArgument);
}

TEST(GameModeKeys, EligibleKeysListsOnlyKeyboardUsages)
{
    for (const auto id : GameModeKeys::eligibleKeys()) {
        const auto& definition = G915KeyCatalog::definition(id);
        EXPECT_TRUE(definition.zone == 0x01 || definition.zone == 0x04) << definition.name;
    }
    EXPECT_GE(GameModeKeys::eligibleKeys().size(), 90U);
}

TEST(OnboardSlot, AcceptsOnlySlotsOneToThree)
{
    for (const int number : {1, 2, 3}) {
        const auto slot = OnboardSlot::create(static_cast<std::uint8_t>(number));
        ASSERT_TRUE(slot.has_value());
        EXPECT_EQ(int{slot->number()}, number);
    }
    for (const int number : {0, 4, 255}) {
        const auto slot = OnboardSlot::create(static_cast<std::uint8_t>(number));
        ASSERT_FALSE(slot.has_value()) << number;
        EXPECT_EQ(slot.error().code, ErrorCode::InvalidArgument);
    }
}
