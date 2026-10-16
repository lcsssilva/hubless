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
    {0x0000, 0}, {0x0000, 1},  {0x0001, 0}, {0x0001, 1}, {0x8071, 0}, {0x8071, 1}, {0x8071, 5},
    {0x8081, 0}, {0x8081, 1},  {0x8081, 6}, {0x8081, 7}, {0x8070, 0}, {0x8070, 1}, {0x8070, 2},
    {0x8070, 3}, {0x8070, 14}, {0x8100, 0}, {0x8100, 2}, {0x8100, 4}, {0x8100, 5},
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
    for (const std::uint16_t feature : std::vector<std::uint16_t>{0x0000, 0x0001, 0x8071, 0x8081, 0x8070, 0x8100}) {
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
    for (std::uint8_t function = 0; function < 4; ++function) {
        EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x4522, function));
    }
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 3));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 7));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8070, 8));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x1234, 0));
    EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8071, 16));
}

TEST(LightingHidppPolicy, OnboardProfilesAllowsOnlyTheReadFunctions)
{
    for (const std::uint8_t function : std::vector<std::uint8_t>{0, 2, 4, 5}) {
        EXPECT_TRUE(LightingHidppPolicy::isAllowed(0x8100, function)) << static_cast<int>(function);
    }
    for (const std::uint8_t function : std::vector<std::uint8_t>{1, 3, 6, 7, 8, 11, 12, 15}) {
        EXPECT_FALSE(LightingHidppPolicy::isAllowed(0x8100, function)) << static_cast<int>(function);
    }
}

TEST(LightingHidppPolicy, ExplicitBlockListCoversTheUnknownHandshake)
{
    EXPECT_TRUE(LightingHidppPolicy::isExplicitlyBlocked(0x4522));
    EXPECT_FALSE(LightingHidppPolicy::isExplicitlyBlocked(0x8071));
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
