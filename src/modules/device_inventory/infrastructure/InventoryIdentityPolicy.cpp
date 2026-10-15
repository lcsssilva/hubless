#include "modules/device_inventory/infrastructure/InventoryIdentityPolicy.hpp"

#include <array>

namespace ghub::device_inventory {

namespace {

constexpr unsigned bit(unsigned function)
{
    return 1U << function;
}

struct Rule {
    std::uint16_t featureId;
    unsigned functions; // máscara: bit N = função N permitida
};

constexpr std::array<Rule, 4> allowedRules{
    Rule{.featureId = 0x0000, .functions = bit(0)},                   // Root GetFeature
    Rule{.featureId = 0x0001, .functions = bit(0) | bit(1)},          // FeatureSet: quantidade e GetFeatureID
    Rule{.featureId = 0x0003, .functions = bit(0) | bit(1)},          // DeviceInfo: entidades e GetFwInfo (sem fn2)
    Rule{.featureId = 0x0005, .functions = bit(0) | bit(1) | bit(2)}, // DeviceNameType: tamanho, nome e tipo
};

} // namespace

bool InventoryIdentityPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept
{
    if (function > 15) {
        return false;
    }
    for (const auto& rule : allowedRules) {
        if (rule.featureId == featureId) {
            return (rule.functions & bit(function)) != 0;
        }
    }
    return false;
}

bool InventoryIdentityPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                        std::span<const std::uint8_t> /*params*/) const noexcept
{
    return isAllowed(featureId, function);
}

} // namespace ghub::device_inventory
