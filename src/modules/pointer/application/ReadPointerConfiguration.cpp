#include "modules/pointer/application/ReadPointerConfiguration.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace ghub::pointer {

namespace {

constexpr auto leaseDuration = std::chrono::seconds{60};

// Dono externo que impede a leitura, se algum estiver ativo.
std::optional<device_access::ExternalOwner> blockingOwner(const std::vector<device_access::ExternalOwner>& owners)
{
    for (const auto owner : owners) {
        if (owner != device_access::ExternalOwner::Piper && owner != device_access::ExternalOwner::Ratbagd) {
            return owner;
        }
    }
    return std::nullopt;
}

PointerStatus unavailable(shared_kernel::DomainError error)
{
    return Unavailable{.error = std::move(error)};
}

PointerStatus pickG703(std::vector<DiscoveredPointer> devices)
{
    for (auto& device : devices) {
        if (!device.usbId.has_value()) {
            continue;
        }
        const auto identified = device_inventory::identifyDevice(*device.usbId);
        if (!identified.has_value() || identified->model != device_inventory::DeviceModel::G703) {
            continue;
        }
        if (!device.configuration.has_value()) {
            return unavailable(std::move(device.configuration.error()));
        }
        return Ready{.configuration = std::move(*device.configuration), .path = identified->path};
    }
    return NotDetected{};
}

} // namespace

PointerStatus ReadPointerConfiguration::execute() const
{
    const auto owners = probe_->runningOwners();
    if (!owners.has_value()) {
        return unavailable(owners.error());
    }
    if (const auto owner = blockingOwner(*owners); owner.has_value()) {
        return unavailable({
            .code = shared_kernel::ErrorCode::DeviceBusy,
            .detail = "em uso por " + std::string{device_access::processName(*owner)},
        });
    }

    auto holder = device_access::LeaseHolder::create("pointer.ratbag");
    if (!holder.has_value()) {
        return unavailable(holder.error());
    }
    const auto lease = registry_->acquire({
        .interfaceId = interfaceId_,
        .holder = std::move(*holder),
        .purpose = "leitura da configuração do mouse",
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return unavailable(lease.error());
    }

    auto devices = backend_->readDevices();
    if (!devices.has_value()) {
        return unavailable(std::move(devices.error()));
    }
    return pickG703(std::move(*devices));
}

} // namespace ghub::pointer
