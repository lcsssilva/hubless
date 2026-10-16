#include "modules/battery/infrastructure/G733BatteryPolicy.hpp"

namespace ghub::battery {

namespace {

constexpr std::uint16_t rootFeature = 0x0000;
constexpr std::uint16_t adcMeasurementFeature = 0x1F20;

} // namespace

bool G733BatteryPolicy::isAllowed(std::uint16_t featureId, std::uint8_t function,
                                  std::span<const std::uint8_t> params) const noexcept
{
    if (function != 0) {
        return false;
    }
    switch (featureId) {
    case rootFeature:
        return params.size() == 2 && params[0] == (adcMeasurementFeature >> 8U) &&
               params[1] == (adcMeasurementFeature & 0xFFU);
    case adcMeasurementFeature:
        return params.empty();
    default:
        return false;
    }
}

} // namespace ghub::battery
