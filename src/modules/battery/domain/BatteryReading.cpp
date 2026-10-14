#include "modules/battery/domain/BatteryReading.hpp"

namespace ghub::battery {

std::optional<BatteryLevel> BatteryReading::currentLevel() const noexcept
{
    if (state_ == ChargeState::Unknown) {
        return std::nullopt;
    }
    return level_;
}

} // namespace ghub::battery
