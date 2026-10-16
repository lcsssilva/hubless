#include "modules/pointer/domain/OnboardButtons.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace ghub::pointer {
namespace {

using Entry = std::array<std::uint8_t, 4>;

// Formatos vistos nas cópias próprias: G703 `80 01 00 NN` (NN = máscara do botão, setores 0x0001-0x0005) e
// `90 05 00 00`; G915 `80 02 MM KK` (C-04/C-06) e `90 00 00 00` (G-shift desativado).
TEST(OnboardButtonsTest, MouseButtonMaskOfTheG703IsDecodedAsButtonNumber)
{
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x01}), ButtonAction::button(1));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x02}), ButtonAction::button(2));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x04}), ButtonAction::button(3));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x08}), ButtonAction::button(4));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x10}), ButtonAction::button(5));
}

TEST(OnboardButtonsTest, KeyWithModifiersCarriesModifiersInTheHighByte)
{
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x02, 0x00, 0x3a}), ButtonAction::key(0x003a)); // F1
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x02, 0x01, 0x06}), ButtonAction::key(0x0106)); // Ctrl+C
}

TEST(OnboardButtonsTest, NinetyZeroZeroZeroIsNoAction)
{
    EXPECT_EQ(decodeOnboardButton(Entry{0x90, 0x00, 0x00, 0x00}), ButtonAction::none());
}

TEST(OnboardButtonsTest, AnyOtherFormatIsKeptAsRawBytes)
{
    EXPECT_EQ(decodeOnboardButton(Entry{0x90, 0x05, 0x00, 0x00}), ButtonAction::unrecognized(0x90050000U));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x03}), ButtonAction::unrecognized(0x80010003U)); // 2 bits
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x01, 0x01}), ButtonAction::unrecognized(0x80010101U));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x03, 0x00, 0x01}), ButtonAction::unrecognized(0x80030001U));
    EXPECT_EQ(decodeOnboardButton(Entry{0x90, 0x00, 0x00, 0x01}), ButtonAction::unrecognized(0x90000001U));
    EXPECT_EQ(decodeOnboardButton(Entry{0x80, 0x01, 0x00, 0x00}), ButtonAction::unrecognized(0x80010000U));
}

TEST(OnboardButtonsTest, RealG703SectorYieldsFiveMouseButtonsAndTheSixthAsRaw)
{
    const auto buttons = decodeOnboardButtons(fixtures::kSector0001);
    ASSERT_EQ(buttons.size(), 6U);
    for (std::uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(buttons[i], ButtonAction::button(i + 1)) << i;
    }
    EXPECT_EQ(buttons[5], ButtonAction::unrecognized(0x90050000U));
}

TEST(OnboardButtonsTest, ListStopsAtTheFirstUnusedEntry)
{
    std::vector<std::uint8_t> sector(255, 0xff);
    sector[0x20] = 0x80;
    sector[0x21] = 0x01;
    sector[0x22] = 0x00;
    sector[0x23] = 0x01;
    EXPECT_EQ(decodeOnboardButtons(sector).size(), 1U);
}

TEST(OnboardButtonsTest, ShortSectorYieldsNoButtonsInsteadOfReadingPastTheEnd)
{
    const std::vector<std::uint8_t> shorter(0x22, 0x00);
    EXPECT_TRUE(decodeOnboardButtons(shorter).empty());
}

} // namespace
} // namespace ghub::pointer
