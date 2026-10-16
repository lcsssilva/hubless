#include "modules/pointer/infrastructure/G703OnboardPolicy.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

using ghub::pointer::infrastructure::G703OnboardPolicy;

namespace {

constexpr std::uint16_t kOnboard = 0x8100;

bool allowed(const G703OnboardPolicy& policy, std::uint16_t feature, std::uint8_t function,
             const std::vector<std::uint8_t>& params = {})
{
    return policy.isAllowed(feature, function, params);
}

std::vector<std::uint8_t> addressParams(unsigned sector, std::uint8_t a2 = 0, std::uint8_t a3 = 0)
{
    return {static_cast<std::uint8_t>(sector >> 8U), static_cast<std::uint8_t>(sector & 0xFFU), a2, a3, 0x00, 0xFF};
}

} // namespace

TEST(G703OnboardPolicyTest, AllowsRootAndFeatureSetQueries)
{
    const G703OnboardPolicy policy{5};
    for (const std::uint16_t feature : std::array<std::uint16_t, 2>{0x0000, 0x0001}) {
        EXPECT_TRUE(allowed(policy, feature, 0));
        EXPECT_TRUE(allowed(policy, feature, 1));
        for (std::uint8_t function = 2; function < 16; ++function) {
            EXPECT_FALSE(allowed(policy, feature, function)) << "feature " << feature << " fn " << int{function};
        }
    }
}

TEST(G703OnboardPolicyTest, AllowsOnlyTheReadFunctionsAndTheWriteTailOfTheOnboardFeature)
{
    const G703OnboardPolicy policy{5};
    for (const std::uint8_t function : std::array<std::uint8_t, 6>{0, 2, 4, 5, 7, 8}) {
        EXPECT_TRUE(allowed(policy, kOnboard, function)) << "fn " << int{function};
    }
    // fn1 (modo), fn3 (perfil atual) e fn9..15 nunca entram.
    for (const std::uint8_t function : std::array<std::uint8_t, 9>{1, 3, 9, 10, 11, 12, 13, 14, 15}) {
        EXPECT_FALSE(allowed(policy, kOnboard, function)) << "fn " << int{function};
    }
}

TEST(G703OnboardPolicyTest, MemoryAddressWriteAcceptsOnlyProfileSectors)
{
    const G703OnboardPolicy policy{5};
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0000))) << "diretório";
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0001)));
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0005)));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0006)));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0100)));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0101))) << "ROM";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0x00, 0x01))) << "endereço diferente de 0";
    EXPECT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0001, 0x01, 0x00))) << "endereço diferente de 0";
}

TEST(G703OnboardPolicyTest, MemoryAddressWriteWithShortOrMissingParamsIsDenied)
{
    const G703OnboardPolicy policy{5};
    EXPECT_FALSE(allowed(policy, kOnboard, 6));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, {0x00, 0x01}));
    EXPECT_FALSE(allowed(policy, kOnboard, 6, {0x00, 0x01, 0x00, 0x00, 0x00}));
}

TEST(G703OnboardPolicyTest, ProfileCountBoundsTheAllowedSectors)
{
    const G703OnboardPolicy none{0};
    EXPECT_FALSE(allowed(none, kOnboard, 6, addressParams(0x0001)));
    const G703OnboardPolicy one{1};
    EXPECT_TRUE(allowed(one, kOnboard, 6, addressParams(0x0001)));
    EXPECT_FALSE(allowed(one, kOnboard, 6, addressParams(0x0002)));
}

TEST(G703OnboardPolicyTest, ProfileCountCanBeFixedAfterTheDescription)
{
    G703OnboardPolicy policy{0};
    ASSERT_FALSE(allowed(policy, kOnboard, 6, addressParams(0x0002)));
    policy.setProfileCount(5);
    EXPECT_TRUE(allowed(policy, kOnboard, 6, addressParams(0x0002)));
}

TEST(G703OnboardPolicyTest, DeniesEveryOtherFeature)
{
    const G703OnboardPolicy policy{5};
    for (const std::uint16_t feature :
         std::array<std::uint16_t, 7>{0x8071, 0x8110, 0x4522, 0x8070, 0x8081, 0x1004, 0x1234}) {
        for (std::uint8_t function = 0; function < 16; ++function) {
            EXPECT_FALSE(allowed(policy, feature, function, addressParams(0x0001)))
                << "feature " << feature << " fn " << int{function};
        }
    }
}

// INC-015 parte 2 (ADR-0025): 0x2201 (DPI) e 0x8060 (report rate) deixaram de ser totalmente recusadas, só para as
// leituras abaixo. O teste DeniesEveryOtherFeature perdeu estas duas features por esse motivo; as funções de escrita
// continuam recusadas nos testes seguintes.
TEST(G703OnboardPolicyTest, AllowsTheSensorDpiReadsOnlyForSensorZero)
{
    const G703OnboardPolicy policy{5};
    EXPECT_TRUE(allowed(policy, 0x2201, 1, {0x00}));
    EXPECT_TRUE(allowed(policy, 0x2201, 2, {0x00}));
}

TEST(G703OnboardPolicyTest, SensorDpiReadsRefuseAnyOtherParams)
{
    const G703OnboardPolicy policy{5};
    for (const std::uint8_t function : {std::uint8_t{1}, std::uint8_t{2}}) {
        EXPECT_FALSE(allowed(policy, 0x2201, function)) << "sem parâmetros";
        EXPECT_FALSE(allowed(policy, 0x2201, function, {0x01})) << "outro sensor";
        EXPECT_FALSE(allowed(policy, 0x2201, function, {0x00, 0x00})) << "parâmetro extra";
        EXPECT_FALSE(allowed(policy, 0x2201, function, {0x00, 0x03, 0x20})) << "tentativa de enviar DPI";
    }
}

TEST(G703OnboardPolicyTest, SensorDpiWriteAndOtherFunctionsStayDenied)
{
    const G703OnboardPolicy policy{5};
    EXPECT_FALSE(allowed(policy, 0x2201, 0, {0x00}));
    EXPECT_FALSE(allowed(policy, 0x2201, 3, {0x00})) << "SetSensorDpi";
    EXPECT_FALSE(allowed(policy, 0x2201, 3, {0x00, 0x03, 0x20}));
    for (std::uint8_t function = 3; function < 16; ++function) {
        EXPECT_FALSE(allowed(policy, 0x2201, function, {0x00})) << "fn " << int{function};
    }
}

TEST(G703OnboardPolicyTest, AllowsTheReportRateReadsWithoutParams)
{
    const G703OnboardPolicy policy{5};
    EXPECT_TRUE(allowed(policy, 0x8060, 0));
    EXPECT_TRUE(allowed(policy, 0x8060, 1));
}

TEST(G703OnboardPolicyTest, ReportRateReadsRefuseAnyParams)
{
    const G703OnboardPolicy policy{5};
    EXPECT_FALSE(allowed(policy, 0x8060, 0, {0x00}));
    EXPECT_FALSE(allowed(policy, 0x8060, 1, {0x02}));
}

TEST(G703OnboardPolicyTest, ReportRateWriteStaysDenied)
{
    const G703OnboardPolicy policy{5};
    EXPECT_FALSE(allowed(policy, 0x8060, 2)) << "SetReportRate";
    EXPECT_FALSE(allowed(policy, 0x8060, 2, {0x02}));
    for (std::uint8_t function = 2; function < 16; ++function) {
        EXPECT_FALSE(allowed(policy, 0x8060, function, {0x02})) << "fn " << int{function};
        EXPECT_FALSE(allowed(policy, 0x8060, function)) << "fn " << int{function};
    }
}
