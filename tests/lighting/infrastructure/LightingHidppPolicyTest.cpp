#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

using ghub::lighting::infrastructure::LightingHidppPolicy;

namespace {

struct Pair {
    std::uint16_t feature;
    std::uint8_t function;
};

const std::vector<Pair> allowed{
    {0x0000, 0}, {0x0000, 1}, {0x0001, 0}, {0x0001, 1}, {0x8071, 0}, {0x8071, 1}, {0x8071, 5}, {0x8081, 0},
    {0x8081, 1}, {0x8081, 6}, {0x8081, 7}, {0x8070, 0}, {0x8070, 1}, {0x8070, 2}, {0x8070, 3}, {0x8070, 14},
    {0x8100, 0}, {0x8100, 2}, {0x8100, 3}, {0x8100, 4}, {0x8100, 5}, {0x4522, 1}, {0x4522, 3},
};

} // namespace

TEST(LightingHidppPolicy, AcceptsEveryListedPair)
{
    for (const auto& pair : allowed) {
        EXPECT_TRUE(LightingHidppPolicy::isAllowed(pair.feature, pair.function))
            << std::hex << pair.feature << " fn" << std::dec << static_cast<int>(pair.function);
    }
}

TEST(LightingHidppPolicy, RejectsEveryOtherFunctionOfTheListedFeatures)
{
    for (const std::uint16_t feature :
         std::vector<std::uint16_t>{0x0000, 0x0001, 0x8071, 0x8081, 0x8070, 0x8100, 0x4522}) {
        for (std::uint8_t function = 0; function < 16; ++function) {
            bool listed = false;
            for (const auto& pair : allowed) {
                listed = listed || (pair.feature == feature && pair.function == function);
            }
            EXPECT_EQ(LightingHidppPolicy::isAllowed(feature, function), listed)
                << std::hex << feature << " fn" << std::dec << static_cast<int>(function);
        }
    }
}

TEST(LightingHidppPolicy, RejectsSampleOfForbiddenPairs)
{
    // 0x4522 (modo de jogo, ADR-0029): só fn1 e fn3; as demais funções continuam recusadas.
    for (const std::uint8_t function : std::vector<std::uint8_t>{0, 2, 4, 5, 15}) {
        EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x4522, function)) << int{function};
    }
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 3));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 7));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8070, 8));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x1234, 0));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 16));
}

TEST(LightingHidppPolicy, OnboardProfilesAllowsOnlyTheReadFunctionsAndTheSlotSwitch)
{
    for (const std::uint8_t function : std::vector<std::uint8_t>{0, 2, 3, 4, 5}) {
        EXPECT_TRUE(LightingHidppPolicy::isAllowed(0x8100, function)) << static_cast<int>(function);
    }
    for (const std::uint8_t function : std::vector<std::uint8_t>{1, 6, 7, 8, 11, 12, 15}) {
        EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8100, function)) << static_cast<int>(function);
    }
}

// INC-015 parte 2 (ADR-0025): leituras de estado do G915.
namespace {

bool allowedWith(std::uint16_t feature, std::uint8_t function, const std::vector<std::uint8_t>& params = {})
{
    const LightingHidppPolicy policy;
    return policy.isAllowed(feature, function, params);
}

} // namespace

TEST(LightingHidppPolicy, StatusReadsAllowOnlyFn0AndFn1OfBrightnessAndReportRate)
{
    for (const std::uint16_t feature : {std::uint16_t{0x8040}, std::uint16_t{0x8060}}) {
        EXPECT_TRUE(allowedWith(feature, 0)) << std::hex << feature;
        EXPECT_TRUE(allowedWith(feature, 1)) << std::hex << feature;
        for (std::uint8_t function = 2; function < 16; ++function) {
            EXPECT_FALSE(allowedWith(feature, function)) << std::hex << feature << " fn" << std::dec << int{function};
        }
    }
}

TEST(LightingHidppPolicy, WriteFunctionsOfBrightnessAndReportRateStayDenied)
{
    EXPECT_FALSE(allowedWith(0x8040, 2));
    EXPECT_FALSE(allowedWith(0x8040, 3));
    EXPECT_FALSE(allowedWith(0x8060, 2));
    EXPECT_FALSE(allowedWith(0x8060, 3));
}

TEST(LightingHidppPolicy, StatusReadsRefuseAnyNonEmptyParams)
{
    EXPECT_FALSE(allowedWith(0x8060, 1, {0x02}));
    EXPECT_FALSE(allowedWith(0x8060, 0, {0x00}));
    EXPECT_FALSE(allowedWith(0x8040, 0, {0x00}));
    EXPECT_FALSE(allowedWith(0x8040, 1, {0x00, 0x32}));
}

// INC-019 (ADR-0029): modo de jogo (0x4522), troca de slot (0x8100 fn3) e brilho.
TEST(LightingHidppPolicy, GameModeClearTakesNoParameters)
{
    EXPECT_TRUE(allowedWith(0x4522, 3));
    EXPECT_FALSE(allowedWith(0x4522, 3, {0x3A}));
    EXPECT_FALSE(allowedWith(0x4522, 3, {0x00}));
}

TEST(LightingHidppPolicy, GameModeSetAcceptsZeroToSixteenCatalogKeyCodes)
{
    EXPECT_TRUE(allowedWith(0x4522, 1)) << "lista vazia reativa todas as teclas";
    EXPECT_TRUE(allowedWith(0x4522, 1, {0x3A})) << "F1 como na captura C-04";
    EXPECT_TRUE(allowedWith(0x4522, 1, {0xE3, 0xE7, 0x65}));
    EXPECT_TRUE(allowedWith(
        0x4522, 1, {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13}));
}

TEST(LightingHidppPolicy, GameModeSetRefusesMoreThanSixteenCodes)
{
    EXPECT_FALSE(allowedWith(0x4522, 1, std::vector<std::uint8_t>(17, 0x04)));
}

TEST(LightingHidppPolicy, GameModeSetRefusesCodesOutsideTheCatalog)
{
    EXPECT_FALSE(allowedWith(0x4522, 1, {0x00}));
    EXPECT_FALSE(allowedWith(0x4522, 1, {0x3A, 0x03}));
    EXPECT_FALSE(allowedWith(0x4522, 1, {0x9B})) << "mídia não é código de teclado";
    EXPECT_FALSE(allowedWith(0x4522, 1, {0xFF}));
}

TEST(LightingHidppPolicy, GameModeOtherFunctionsStayDeniedWithAnyParams)
{
    EXPECT_FALSE(allowedWith(0x4522, 0));
    EXPECT_FALSE(allowedWith(0x4522, 2, {0x3A}));
    EXPECT_FALSE(allowedWith(0x4522, 4));
}

TEST(LightingHidppPolicy, SlotSwitchAcceptsOnlySlotsOneToThree)
{
    for (const int slot : {1, 2, 3}) {
        EXPECT_TRUE(allowedWith(0x8100, 3, {0x00, static_cast<std::uint8_t>(slot)})) << slot;
    }
    EXPECT_FALSE(allowedWith(0x8100, 3, {0x00, 0x00}));
    EXPECT_FALSE(allowedWith(0x8100, 3, {0x00, 0x04}));
    EXPECT_FALSE(allowedWith(0x8100, 3, {0x01, 0x01})) << "o byte alto do setor é 00";
    EXPECT_FALSE(allowedWith(0x8100, 3, {0x00}));
    EXPECT_FALSE(allowedWith(0x8100, 3, {}));
    EXPECT_FALSE(allowedWith(0x8100, 3, {0x00, 0x01, 0x00}));
}

TEST(LightingHidppPolicy, WriteFunctionsOfOnboardMemoryStayDeniedForLighting)
{
    for (const int function : {6, 7, 8}) {
        EXPECT_FALSE(allowedWith(0x8100, static_cast<std::uint8_t>(function), std::vector<std::uint8_t>(6, 0)))
            << function;
    }
}

TEST(LightingHidppPolicy, BrightnessFeatureStaysReadOnly)
{
    EXPECT_FALSE(allowedWith(0x8040, 2, {0x32}));
    EXPECT_FALSE(allowedWith(0x8040, 3));
    EXPECT_FALSE(allowedWith(0x8040, 3, {0x00, 0x32}));
}
