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

TEST(G733StatusPolicyTest, DeniesTheSidetoneWriteAndEveryOtherFunction)
{
    EXPECT_FALSE(allowed(0x8300, 1)) << "escrita de sidetone";
    EXPECT_FALSE(allowed(0x8300, 1, {0x32}));
    for (std::uint8_t function = 1; function < 16; ++function) {
        EXPECT_FALSE(allowed(0x8300, function)) << "0x8300 fn " << int{function};
        EXPECT_FALSE(allowed(0x8310, function)) << "0x8310 fn " << int{function};
        EXPECT_FALSE(allowed(0x8310, function, {0x00, 0x00})) << "0x8310 fn " << int{function};
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
