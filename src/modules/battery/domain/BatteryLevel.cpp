#include "modules/battery/domain/BatteryLevel.hpp"

namespace ghub::battery {

shared_kernel::Result<BatteryLevel> BatteryLevel::fromPercent(int percent)
{
    if (percent < 0 || percent > 100) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "percentual fora de 0-100");
    }
    return BatteryLevel{percent};
}

} // namespace ghub::battery
