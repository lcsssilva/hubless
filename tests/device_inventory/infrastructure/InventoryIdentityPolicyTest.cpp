#include "modules/device_inventory/infrastructure/InventoryIdentityPolicy.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace ghub::device_inventory {
namespace {

struct Pair {
    std::uint16_t feature;
    std::uint8_t function;
};

// Lista fechada de INC-015 R2.
const std::vector<Pair> allowed{{0x0000, 0}, {0x0001, 0}, {0x0001, 1}, {0x0003, 0},
                                {0x0003, 1}, {0x0005, 0}, {0x0005, 1}, {0x0005, 2}};

TEST(InventoryIdentityPolicy, AcceptsEveryListedPair)
{
    const InventoryIdentityPolicy policy;
    for (const auto& pair : allowed) {
        EXPECT_TRUE(policy.isAllowed(pair.feature, pair.function, {}))
            << std::hex << pair.feature << " fn" << std::dec << static_cast<int>(pair.function);
    }
}

TEST(InventoryIdentityPolicy, RejectsEveryOtherFunctionOfTheListedFeatures)
{
    const InventoryIdentityPolicy policy;
    for (const std::uint16_t feature : std::vector<std::uint16_t>{0x0000, 0x0001, 0x0003, 0x0005}) {
        for (unsigned function = 0; function < 256; ++function) {
            bool listed = false;
            for (const auto& pair : allowed) {
                listed = listed || (pair.feature == feature && pair.function == function);
            }
            EXPECT_EQ(policy.isAllowed(feature, static_cast<std::uint8_t>(function), {}), listed)
                << std::hex << feature << " fn" << std::dec << function;
        }
    }
}

TEST(InventoryIdentityPolicy, RejectsTheSerialNumberFunctionOfDeviceInfo)
{
    const InventoryIdentityPolicy policy;

    EXPECT_FALSE(policy.isAllowed(0x0003, 2, {}));
}

TEST(InventoryIdentityPolicy, RejectsOnboardProfilesAndTheBlockedFeature)
{
    const InventoryIdentityPolicy policy;
    for (unsigned function = 0; function < 16; ++function) {
        EXPECT_FALSE(policy.isAllowed(0x8100, static_cast<std::uint8_t>(function), {})) << function;
        EXPECT_FALSE(policy.isAllowed(0x4522, static_cast<std::uint8_t>(function), {})) << function;
    }
}

TEST(InventoryIdentityPolicy, RejectsEveryOtherFeature)
{
    const InventoryIdentityPolicy policy;
    for (const std::uint16_t feature : std::vector<std::uint16_t>{0x0002, 0x0004, 0x0006, 0x1000, 0x1004, 0x1b04,
                                                                  0x8070, 0x8071, 0x8081, 0x8040, 0x8060, 0xFFFF}) {
        for (unsigned function = 0; function < 16; ++function) {
            EXPECT_FALSE(policy.isAllowed(feature, static_cast<std::uint8_t>(function), {}))
                << std::hex << feature << " fn" << std::dec << function;
        }
    }
}

TEST(InventoryIdentityPolicy, IgnoresTheParametersOfAnAllowedPair)
{
    const InventoryIdentityPolicy policy;
    const std::array<std::uint8_t, 2> params{0xFF, 0xFF};

    EXPECT_TRUE(policy.isAllowed(0x0005, 1, params));
}

} // namespace
} // namespace ghub::device_inventory
