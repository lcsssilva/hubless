#include "modules/battery/domain/LowBatteryAlert.hpp"

namespace ghub::battery {

shared_kernel::Result<LowBatteryThreshold> LowBatteryThreshold::create(int percent)
{
    if (percent < minimumPercent || percent > maximumPercent) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "o limiar do aviso de bateria deve estar entre 5 e 50 %");
    }
    return LowBatteryThreshold{percent};
}

bool LowBatteryAlert::observe(ChargeState state, std::optional<BatteryLevel> level) noexcept
{
    if (!enabled_) {
        return false;
    }
    if (state == ChargeState::Charging || state == ChargeState::Full) {
        notified_ = false;
        return false;
    }
    if (!level.has_value()) {
        return false;
    }
    const int percent = level->percent();
    if (percent >= threshold_.percent() + rearmMargin) {
        notified_ = false;
        return false;
    }
    if (percent <= threshold_.percent() && !notified_) {
        notified_ = true;
        return true;
    }
    return false;
}

} // namespace ghub::battery
