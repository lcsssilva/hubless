#include "modules/lighting/application/KeyboardAccess.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace ghub::lighting::application::detail {

namespace {

constexpr std::string_view leaseHolder = "lighting.hidpp";

// Donos externos que abrem o receptor do teclado (INC-008 R6). O HeadsetControl só fala com o G733.
bool blocksKeyboard(device_access::ExternalOwner owner)
{
    return owner != device_access::ExternalOwner::HeadsetControl;
}

} // namespace

shared_kernel::Result<void> ensureNoExternalOwner(device_access::ExternalProcessProbe& probe)
{
    const auto owners = probe.runningOwners();
    if (!owners.has_value()) {
        return std::unexpected(owners.error());
    }
    for (const auto owner : *owners) {
        if (blocksKeyboard(owner)) {
            return shared_kernel::fail(shared_kernel::ErrorCode::DeviceBusy,
                                       "em uso por " + std::string{device_access::processName(owner)});
        }
    }
    return {};
}

shared_kernel::Result<device_access::LeaseRequest>
keyboardLeaseRequest(const device_access::PhysicalInterfaceId& interfaceId, std::string purpose)
{
    auto holder = device_access::LeaseHolder::create(std::string{leaseHolder});
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    return device_access::LeaseRequest{
        .interfaceId = interfaceId,
        .holder = std::move(*holder),
        .purpose = std::move(purpose),
        .maxDuration = keyboardLeaseDuration,
    };
}

} // namespace ghub::lighting::application::detail
