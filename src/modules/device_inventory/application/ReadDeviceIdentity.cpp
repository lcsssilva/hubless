#include "modules/device_inventory/application/ReadDeviceIdentity.hpp"

#include <chrono>
#include <string>
#include <utility>

namespace ghub::device_inventory {

namespace {

constexpr auto leaseHolder = "inventory.identity";
constexpr std::chrono::seconds leaseDuration{10};

const char* interfaceFor(IdentityTarget target)
{
    switch (target) {
    case IdentityTarget::G915:
        return "hidpp:046d:407c";
    case IdentityTarget::G703:
        return "hidpp:046d:c539";
    case IdentityTarget::G733:
        return "hidpp:046d:0afe";
    }
    return "";
}

} // namespace

shared_kernel::Result<DeviceIdentity> ReadDeviceIdentity::execute(IdentityTarget target) const
{
    auto interfaceId = device_access::PhysicalInterfaceId::create(interfaceFor(target));
    if (!interfaceId.has_value()) {
        return std::unexpected(interfaceId.error());
    }
    auto holder = device_access::LeaseHolder::create(leaseHolder);
    if (!holder.has_value()) {
        return std::unexpected(holder.error());
    }
    const auto lease = registry_->acquire({
        .interfaceId = std::move(*interfaceId),
        .holder = std::move(*holder),
        .purpose = "ler identidade HID++",
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    // `lease` vive até o fim do escopo: a interface fica reservada durante a leitura e é liberada na saída.
    return source_->read(target);
}

} // namespace ghub::device_inventory
