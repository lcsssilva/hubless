#include "modules/device_access/domain/LeaseRequest.hpp"

namespace ghub::device_access {

shared_kernel::Result<LeaseHolder> LeaseHolder::create(std::string name)
{
    if (name.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "LeaseHolder vazio");
    }
    return LeaseHolder{std::move(name)};
}

} // namespace ghub::device_access
