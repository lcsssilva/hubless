#include "modules/device_access/infrastructure/SteadyClock.hpp"

namespace ghub::device_access {

shared_kernel::Instant SteadyClock::now() const
{
    return std::chrono::steady_clock::now();
}

} // namespace ghub::device_access
