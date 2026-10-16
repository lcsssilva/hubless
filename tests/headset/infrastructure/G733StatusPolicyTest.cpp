#include "modules/headset/infrastructure/G733StatusPolicy.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using ghub::headset::infrastructure::G733StatusPolicy;

namespace {

bool allowed(std::uint16_t feature, std::uint8_t function, const std::vector<std::uint8_t>& params = {})
{
    const G733StatusPolicy policy;
    return policy.isAllowed(feature, function, params);
}

} // namespace

TEST(G733StatusPolicyTest, AllowsRootGetFeatureWithTheTwoByteQuery)
{
    EXPECT_TRUE(allowed(0x0000, 0, {0x83, 0x10}));
    EXPECT_TRUE(allowed(0x0000, 0, {0x83, 0x00}));
}

TEST(G733StatusPolicyTest, RootAllowsOnlyFn0WithAFeatureIdQuery)
{
    EXPECT_FALSE(allowed(0x0000, 0, {}));
    EXPECT_FALSE(allowed(0x0000, 0, {0x83}));
    EXPECT_FALSE(allowed(0x0000, 0, {0x83, 0x10, 0x00}));
    for (std::uint8_t function = 1; function < 16; ++function) {
        EXPECT_FALSE(allowed(0x0000, function, {0x83, 0x10})) << "fn " << int{function};
    }
}

TEST(G733StatusPolicyTest, AllowsTheEqualizerBandCountAndTheUninterpretedReadWithoutParams)
{
    EXPECT_TRUE(allowed(0x8310, 0));
    EXPECT_TRUE(allowed(0x8300, 0));
}

TEST(G733StatusPolicyTest, DeniesEveryOtherFunctionOfTheReadFeatures)
{
    for (std::uint8_t function = 1; function < 16; ++function) {
        if (function != 1) {
            EXPECT_FALSE(allowed(0x8300, function)) << "0x8300 fn " << int{function};
            EXPECT_FALSE(allowed(0x8300, function, {0x32})) << "0x8300 fn " << int{function};
        }
        EXPECT_FALSE(allowed(0x8310, function)) << "0x8310 fn " << int{function} << ": escrita do EQ recusada";
        EXPECT_FALSE(allowed(0x8310, function, {0x00, 0x00})) << "0x8310 fn " << int{function};
    }
}

TEST(G733StatusPolicyTest, AllowsTheSidetoneWriteWithExactlyOneByteUpToOneHundred)
{
    EXPECT_TRUE(allowed(0x8300, 1, {0x00}));
    EXPECT_TRUE(allowed(0x8300, 1, {0x32}));
    EXPECT_TRUE(allowed(0x8300, 1, {0x63}));
    EXPECT_TRUE(allowed(0x8300, 1, {0x64}));
}

TEST(G733StatusPolicyTest, DeniesSidetoneAbove100OrWithTheWrongLength)
{
    EXPECT_FALSE(allowed(0x8300, 1, {0x65})) << "101";
    EXPECT_FALSE(allowed(0x8300, 1, {0xFF}));
    EXPECT_FALSE(allowed(0x8300, 1, {}));
    EXPECT_FALSE(allowed(0x8300, 1, {0x32, 0x00}));
}

TEST(G733StatusPolicyTest, AllowsTheFourLightingFormatsOfTheCaptures)
{
    EXPECT_TRUE(allowed(0x8070, 3, {0x00, 0x00})) << "desligado, zona inferior";
    EXPECT_TRUE(allowed(0x8070, 3, {0x01, 0x00})) << "desligado, zona superior";
    EXPECT_TRUE(allowed(0x8070, 3, {0x01, 0x01, 0x00, 0xFF, 0xFF, 0x02})) << "fixo (C-03)";
    EXPECT_TRUE(allowed(0x8070, 3, {0x01, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x00, 0x64})) << "respiração (C-03)";
    EXPECT_TRUE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x31})) << "ciclo (C-03)";
    EXPECT_TRUE(allowed(0x8070, 3, {0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x64})) << "ciclo (C-01)";
}

TEST(G733StatusPolicyTest, DeniesLightingOutsideTheFormats)
{
    EXPECT_FALSE(allowed(0x8070, 3, {})) << "sem zona";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00})) << "sem efeito";
    EXPECT_FALSE(allowed(0x8070, 3, {0x02, 0x00})) << "zona 2";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x04})) << "efeito 4";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x00, 0x00})) << "desligado com parâmetros";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x01, 0x00, 0xFF, 0xFF})) << "fixo curto";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x01, 0x00, 0xFF, 0xFF, 0x03})) << "fixo sem o byte 02";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x01, 0x64})) << "respiração, byte 00";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x00})) << "respiração curta";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x64})) << "ciclo, cor";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88})) << "ciclo curto";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x65})) << "brilho 101";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x00})) << "brilho 0";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE7, 0x64})) << "período 999";
    EXPECT_FALSE(allowed(0x8070, 3, {0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x21, 0x64})) << "período 20001";
}

TEST(G733StatusPolicyTest, DeniesTheLightingFlagsAndEveryOtherLightingFunction)
{
    EXPECT_FALSE(allowed(0x8070, 7)) << "sinalizador do G HUB ao conectar";
    EXPECT_FALSE(allowed(0x8070, 8)) << "sinalizador do G HUB ao conectar";
    EXPECT_FALSE(allowed(0x8070, 7, {0x01, 0x01}));
    EXPECT_FALSE(allowed(0x8070, 8, {0x01, 0x01}));
    for (std::uint8_t function = 0; function < 16; ++function) {
        if (function != 3) {
            EXPECT_FALSE(allowed(0x8070, function, {0x00, 0x00})) << "0x8070 fn " << int{function};
            EXPECT_FALSE(allowed(0x8070, function)) << "0x8070 fn " << int{function};
        }
    }
}

TEST(G733StatusPolicyTest, DeniesTheBatteryAndStillDeniesTheStoredEqualizerWriteAndFeature8010)
{
    for (std::uint8_t function = 0; function < 16; ++function) {
        EXPECT_FALSE(allowed(0x8010, function)) << "0x8010 fn " << int{function};
        EXPECT_FALSE(allowed(0x1f20, function)) << "a bateria é do módulo battery";
    }
}

TEST(G733StatusPolicyTest, ReadsRefuseAnyNonEmptyParams)
{
    EXPECT_FALSE(allowed(0x8300, 0, {0x32}));
    EXPECT_FALSE(allowed(0x8300, 0, {0x00}));
    EXPECT_FALSE(allowed(0x8310, 0, {0x01}));
}

TEST(G733StatusPolicyTest, DeniesEveryOtherFeature)
{
    for (const std::uint16_t feature :
         {std::uint16_t{0x0001}, std::uint16_t{0x0003}, std::uint16_t{0x0005}, std::uint16_t{0x1000},
          std::uint16_t{0x8100}, std::uint16_t{0x8071}, std::uint16_t{0x4522}, std::uint16_t{0x1234}}) {
        for (std::uint8_t function = 0; function < 16; ++function) {
            EXPECT_FALSE(allowed(feature, function)) << std::hex << feature << " fn " << std::dec << int{function};
        }
    }
}
