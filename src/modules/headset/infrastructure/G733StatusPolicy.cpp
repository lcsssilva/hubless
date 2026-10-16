#include "modules/headset/infrastructure/G733StatusPolicy.hpp"

namespace ghub::headset::infrastructure {

namespace {

constexpr std::uint16_t rootFeature = 0x0000;
constexpr std::uint16_t equalizerFeature = 0x8310;
constexpr std::uint16_t uninterpretedFeature = 0x8300;
constexpr std::size_t featureIdQuerySize = 2;

} // namespace

bool G733StatusPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept
{
    if (function != 0) {
        return false;
    }
    switch (featureId) {
    case rootFeature:
        return params.size() == featureIdQuerySize;
    case equalizerFeature:
    case uninterpretedFeature:
        return params.empty();
    default:
        return false;
    }
}

} // namespace ghub::headset::infrastructure
