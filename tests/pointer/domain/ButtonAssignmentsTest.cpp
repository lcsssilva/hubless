#include "modules/pointer/domain/ButtonAssignments.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ghub::pointer {
namespace {

using shared_kernel::ErrorCode;
using Bytes = std::vector<std::uint8_t>;
using Entry = std::array<std::uint8_t, 4>;

Bytes sector0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}

std::vector<ButtonAssignment> allKeep()
{
    return std::vector<ButtonAssignment>(ButtonAssignments::kCount, ButtonAssignment::keep());
}

Entry entryAt(const Bytes& sector, std::size_t button)
{
    Entry entry{};
    std::copy_n(sector.begin() + 0x20 + static_cast<std::ptrdiff_t>(button * 4), 4, entry.begin());
    return entry;
}

TEST(ButtonAssignmentTest, MouseButtonEncodesTheSingleBitMask)
{
    EXPECT_EQ(ButtonAssignment::mouseButton(1)->entry(), (Entry{0x80, 0x01, 0x00, 0x01}));
    EXPECT_EQ(ButtonAssignment::mouseButton(3)->entry(), (Entry{0x80, 0x01, 0x00, 0x04}));
    EXPECT_EQ(ButtonAssignment::mouseButton(5)->entry(), (Entry{0x80, 0x01, 0x00, 0x10}));
}

TEST(ButtonAssignmentTest, MouseButtonOutsideOneToFiveIsRefused)
{
    for (const std::uint32_t number : {0U, 6U, 8U, 255U}) {
        const auto result = ButtonAssignment::mouseButton(number);
        ASSERT_FALSE(result.has_value()) << number;
        EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
    }
}

// Evidência C-06: Ctrl+C = 80 02 01 06.
TEST(ButtonAssignmentTest, KeyWithModifiersEncodesTheCtrlCBytesOfTheCapture)
{
    EXPECT_EQ(ButtonAssignment::key(0x01, 0x06)->entry(), (Entry{0x80, 0x02, 0x01, 0x06}));
    EXPECT_EQ(ButtonAssignment::key(0x00, 0x3a)->entry(), (Entry{0x80, 0x02, 0x00, 0x3a})); // F1
}

TEST(ButtonAssignmentTest, KeyOutsideTheCatalogOrWithInvalidModifiersIsRefused)
{
    EXPECT_FALSE(ButtonAssignment::key(0x00, 0x00).has_value());
    EXPECT_FALSE(ButtonAssignment::key(0x00, 0x01).has_value());
    EXPECT_FALSE(ButtonAssignment::key(0x00, 0xe0).has_value()); // modificador como tecla
    EXPECT_FALSE(ButtonAssignment::key(0x00, 0xff).has_value());
    EXPECT_FALSE(ButtonAssignment::key(0x10, 0x06).has_value()); // bit fora dos quatro modificadores
    EXPECT_FALSE(ButtonAssignment::key(0x100, 0x06).has_value());
    EXPECT_FALSE(ButtonAssignment::key(0x00, 0x106).has_value());
    EXPECT_EQ(ButtonAssignment::key(0x10, 0x06).error().code, ErrorCode::InvalidArgument);
}

TEST(ButtonAssignmentTest, CatalogHasLettersDigitsFunctionNavigationAndEditingKeysOnly)
{
    const auto catalog = onboardKeyCatalog();
    const auto has = [&](std::uint8_t code) {
        return std::ranges::any_of(catalog, [code](const OnboardKey& key) { return key.code == code; });
    };
    // 26 letras + 10 dígitos + 12 F + Enter/Esc/Backspace/Tab/Espaço + 4 setas + Home/End/PgUp/PgDn + Insert/Delete
    EXPECT_EQ(catalog.size(), 26U + 10U + 12U + 5U + 4U + 4U + 2U);
    for (std::uint8_t code = 0x04; code <= 0x27; ++code) { // A..Z, 1..9, 0
        EXPECT_TRUE(has(code)) << int{code};
    }
    for (std::uint8_t code = 0x3a; code <= 0x45; ++code) { // F1..F12
        EXPECT_TRUE(has(code)) << int{code};
    }
    EXPECT_FALSE(has(0x39)); // Caps Lock
    EXPECT_FALSE(has(0x46)); // Print Screen
    EXPECT_FALSE(has(0x65)); // Application
    for (const OnboardKey& key : catalog) {
        EXPECT_FALSE(key.label.empty());
    }
}

TEST(ButtonAssignmentsTest, NeedsAReferenceOnlyWhenSomeButtonUsesTheFactoryDefault)
{
    EXPECT_FALSE(ButtonAssignments::keepAll().needsReference());
    auto list = allKeep();
    list[2] = ButtonAssignment::factoryDefault();
    EXPECT_TRUE(ButtonAssignments::make(list, false)->needsReference());
}

TEST(ButtonAssignmentsTest, WrongNumberOfButtonsIsRefused)
{
    auto list = allKeep();
    list.pop_back();
    const auto result = ButtonAssignments::make(list, false);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
}

// R3: o botão 1 não pode ficar sem clique sem confirmação explícita.
TEST(ButtonAssignmentsTest, PrimaryButtonReassignedToAnythingElseNeedsConfirmation)
{
    auto list = allKeep();
    list[0] = *ButtonAssignment::key(0x00, 0x06);
    const auto refused = ButtonAssignments::make(list, false);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::InvalidArgument);
    EXPECT_NE(refused.error().detail.find("botão 1"), std::string::npos);
    EXPECT_TRUE(ButtonAssignments::make(list, true).has_value());

    list[0] = *ButtonAssignment::mouseButton(2);
    EXPECT_FALSE(ButtonAssignments::make(list, false).has_value());
}

TEST(ButtonAssignmentsTest, PrimaryButtonKeptOrSetToMouseOneOrDefaultNeedsNoConfirmation)
{
    auto list = allKeep();
    EXPECT_TRUE(ButtonAssignments::make(list, false).has_value());
    list[0] = *ButtonAssignment::mouseButton(1);
    EXPECT_TRUE(ButtonAssignments::make(list, false).has_value());
    list[0] = ButtonAssignment::factoryDefault();
    EXPECT_TRUE(ButtonAssignments::make(list, false).has_value());
}

TEST(ButtonAssignmentsTest, ApplyChangesOnlyTheEditedEntriesOfTheRealSectorByteForByte)
{
    auto list = allKeep();
    list[3] = *ButtonAssignment::key(0x01, 0x06); // Ctrl+C no botão 4
    list[4] = *ButtonAssignment::mouseButton(2);  // botão 5 vira botão 2
    const auto assignments = ButtonAssignments::make(list, false).value();
    const Bytes before = sector0001();
    const auto after = assignments.applyTo(before, before);
    ASSERT_TRUE(after.has_value());
    ASSERT_EQ(after->size(), before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        const bool edited = (i >= 0x2c && i < 0x34);
        if (!edited) {
            EXPECT_EQ((*after)[i], before[i]) << "byte " << i;
        }
    }
    EXPECT_EQ(entryAt(*after, 3), (Entry{0x80, 0x02, 0x01, 0x06}));
    EXPECT_EQ(entryAt(*after, 4), (Entry{0x80, 0x01, 0x00, 0x02}));
    EXPECT_EQ(entryAt(*after, 5), (Entry{0x90, 0x05, 0x00, 0x00})); // não interpretada, preservada
}

TEST(ButtonAssignmentsTest, FactoryDefaultCopiesTheReferenceEntryNotTheCurrentOne)
{
    Bytes current = sector0001();
    current[0x23] = 0x04; // botão 1 atual = botão 3
    auto list = allKeep();
    list[0] = ButtonAssignment::factoryDefault();
    list[5] = ButtonAssignment::factoryDefault();
    const auto after = ButtonAssignments::make(list, false)->applyTo(current, sector0001());
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(entryAt(*after, 0), (Entry{0x80, 0x01, 0x00, 0x01}));
    EXPECT_EQ(entryAt(*after, 5), (Entry{0x90, 0x05, 0x00, 0x00}));
}

TEST(ButtonAssignmentsTest, KeepAllLeavesTheSectorIdentical)
{
    const Bytes before = sector0001();
    EXPECT_EQ(ButtonAssignments::keepAll().applyTo(before, before).value(), before);
}

TEST(ButtonAssignmentsTest, ShortSectorOrReferenceIsRefused)
{
    const Bytes shortSector(0x30, 0x00);
    const Bytes full = sector0001();
    EXPECT_FALSE(ButtonAssignments::keepAll().applyTo(shortSector, full).has_value());
    EXPECT_FALSE(ButtonAssignments::keepAll().applyTo(full, shortSector).has_value());
}

TEST(ButtonAssignmentsTest, ReadReturnsTheSixInterpretedButtonsOfTheRealSector)
{
    const auto buttons = ButtonAssignments::read(sector0001());
    ASSERT_TRUE(buttons.has_value());
    ASSERT_EQ(buttons->size(), ButtonAssignments::kCount);
    EXPECT_EQ((*buttons)[0], ButtonAction::button(1));
    EXPECT_EQ((*buttons)[4], ButtonAction::button(5));
    EXPECT_EQ((*buttons)[5], ButtonAction::unrecognized(0x90050000U));
}

TEST(ButtonAssignmentsTest, ReadRefusesASectorWithoutSixUsedEntries)
{
    Bytes sector = sector0001();
    std::fill(sector.begin() + 0x2c, sector.begin() + 0x30, std::uint8_t{0xff});
    EXPECT_FALSE(ButtonAssignments::read(sector).has_value());
    EXPECT_FALSE(ButtonAssignments::read(Bytes(0x20, 0)).has_value());
}

} // namespace
} // namespace ghub::pointer
