#include "modules/lighting/infrastructure/G915OnboardPolicy.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

// Política dos comandos HID++ da memória onboard do G915 (INC-019 R5, ADR-0029): fn6/fn7/fn8 só no formato da captura
// C-06 (`<setor BE16> 00 00 00 ff`, 16 bytes por fn7, fn8 vazia) e só para os setores de perfil 1..3.
namespace {

using ghub::lighting::infrastructure::G915OnboardPolicy;

constexpr std::uint16_t kOnboard = 0x8100;

bool allowed(const G915OnboardPolicy& policy, std::uint16_t feature, std::uint8_t function,
             const std::vector<std::uint8_t>& params = {})
{
    return policy.isAllowed(feature, function, params);
}

// fn6 como o G HUB enviou na C-06: setor, endereço 0000, tamanho 00ff.
std::vector<std::uint8_t> addressParams(unsigned sector, std::uint8_t a2 = 0, std::uint8_t a3 = 0,
                                        std::uint8_t sizeHigh = 0x00, std::uint8_t sizeLow = 0xFF)
{
    return {static_cast<std::uint8_t>(sector >> 8U), static_cast<std::uint8_t>(sector & 0xFFU), a2, a3, sizeHigh,
            sizeLow};
}

} // namespace

TEST(G915OnboardPolicy, AllowsRootAndFeatureSetQueries)
{
    const G915OnboardPolicy policy{3};
    for (const std::uint16_t feature : std::array<std::uint16_t, 2>{0x0000, 0x0001}) {
        EXPECT_TRUE(allowed(policy, feature, 0));
        EXPECT_TRUE(allowed(policy, feature, 1));
        for (std::uint8_t function = 2; function < 16; ++function) {
            EXPECT_FALSE(allowed(policy, feature, function)) << "feature " << feature << " fn " << int{function};
        }
    }
}

TEST(G915OnboardPolicy, AllowsTheReadFunctionsOfTheOnboardFeature)
{
    const G915OnboardPolicy policy{3};
    for (const std::uint8_t function : std::array<std::uint8_t, 4>{0, 2, 4, 5}) {
        EXPECT_TRUE(allowed(policy, kOnboard, function)) << "fn " << int{function};
    }
}

TEST(G915OnboardPolicy, DeniesModeSwitchSlotSwitchAndEverythingElseOfTheOnboardFeature)
{
    const G915OnboardPolicy policy{3};
    // fn1 (modo), fn3 (troca de slot, que pertence ao ControlKeyboard) e fn9..15 nunca entram.
    for (const std::uint8_t function : std::array<std::uint8_t, 9>{1, 3, 9, 10, 11, 12, 13, 14, 15}) {
        EXPECT_FALSE(allowed(policy, kOnboard, function, {0x00, 0x01})) << "fn " << int{function};
    }
}

TEST(G915OnboardPolicy, MemoryAddressWriteAcceptsOnlyTheThreeProfileSectors)
{
    const G915OnboardPolicy policy{3};
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0000))) << "diretório";
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0001)));
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0002)));
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0003)));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0004)));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0101))) << "ROM";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0100)));
}

TEST(G915OnboardPolicy, MemoryAddressWriteNeedsTheExactCaptureFormat)
{
    const G915OnboardPolicy policy{3};
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0x00, 0x01))) << "endereço diferente de 0";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0x01, 0x00))) << "endereço diferente de 0";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0, 0, 0x00, 0x10))) << "tamanho diferente de 255";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0, 0, 0x01, 0xFF))) << "tamanho diferente de 255";
    EXPECT_FALSE(allowed(policy, kOnboard, 6));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, {0x00, 0x01}));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, {0x00, 0x01, 0x00, 0x00, 0x00}));
    auto longer = addressParams(0x0001);
    longer.push_back(0x00);
    EXPECT_FALSE(allowed(policy, kOnboard, 6, longer));
}

TEST(G915OnboardPolicy, ProfileCountBoundsTheAllowedSectorsAndNeverPassesThree)
{
    const G915OnboardPolicy none{0};
    EXPECT_FALSE(allowed(none, kOnboard, 6, addressParams(0x0001))) << "antes do describe()";
    const G915OnboardPolicy one{1};
    EXPECT_TRUE(allowed(one, kOnboard, 6, addressParams(0x0001)));
    EXPECT_FALSE(allowed(one, kOnboard, 6, addressParams(0x0002)));
    const G915OnboardPolicy five{5}; // um dispositivo que se anuncie com 5 perfis continua limitado ao M1-M3
    EXPECT_TRUE(allowed(five, kOnboard, 6, addressParams(0x0003)));
    EXPECT_FALSE(allowed(five, kOnboard, 6, addressParams(0x0004)));
}

TEST(G915OnboardPolicy, ProfileCountCanBeFixedAfterTheDescription)
{
    G915OnboardPolicy policy{0};
    ASSERT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0002)));
    policy.setProfileCount(3);
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0002)));
}

TEST(G915OnboardPolicy, MemoryWriteBodyIsExactlySixteenBytesAndTheEndHasNoParams)
{
    const G915OnboardPolicy policy{3};
    EXPECT_TRUE(allowed(policy, kOnboard, 7, std::vector<std::uint8_t>(16, 0x00)));
    EXPECT_FALSE(allowed(policy, kOnboard, 7));
    EXPECT_FALSE(allowed(policy, kOnboard, 7, std::vector<std::uint8_t>(15, 0x00)));
    EXPECT_FALSE(allowed(policy, kOnboard, 7, std::vector<std::uint8_t>(17, 0x00)));
    EXPECT_TRUE(allowed(policy, kOnboard, 8));
    EXPECT_FALSE(allowed(policy, kOnboard, 8, {0x00}));
}

TEST(G915OnboardPolicy, DeniesEveryOtherFeature)
{
    const G915OnboardPolicy policy{3};
    for (const std::uint16_t feature :
         std::array<std::uint16_t, 9>{0x8071, 0x8110, 0x4522, 0x8070, 0x8081, 0x8040, 0x8060, 0x2201, 0x1234}) {
        for (std::uint8_t function = 0; function < 16; ++function) {
            EXPECT_FALSE(allowed(policy, feature, function, addressParams(0x0001)))
                << "feature " << feature << " fn " << int{function};
            EXPECT_FALSE(allowed(policy, feature, function)) << "feature " << feature << " fn " << int{function};
        }
    }
}
