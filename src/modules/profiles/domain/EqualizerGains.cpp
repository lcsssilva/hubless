#include "modules/profiles/domain/EqualizerGains.hpp"

#include <algorithm>

namespace ghub::profiles {

shared_kernel::Result<EqualizerGains> EqualizerGains::create(const std::vector<int>& gains)
{
    if (gains.size() != bandCount) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "a curva do EQ precisa de 10 ganhos");
    }
    std::array<int, bandCount> values{};
    std::ranges::copy(gains, values.begin());
    return EqualizerGains{values};
}

} // namespace ghub::profiles
