#include "modules/headset/domain/SidetoneLevel.hpp"

namespace ghub::headset {

shared_kernel::Result<SidetoneLevel> SidetoneLevel::create(int value)
{
    if (value < minimum || value > maximum) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "sidetone fora de 0-100");
    }
    return SidetoneLevel{value};
}

} // namespace ghub::headset
