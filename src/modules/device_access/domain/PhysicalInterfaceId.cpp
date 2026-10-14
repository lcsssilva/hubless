#include "modules/device_access/domain/PhysicalInterfaceId.hpp"

namespace ghub::device_access {

shared_kernel::Result<PhysicalInterfaceId> PhysicalInterfaceId::create(std::string value)
{
    if (value.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "PhysicalInterfaceId vazio");
    }
    return PhysicalInterfaceId{std::move(value)};
}

} // namespace ghub::device_access
