#include "modules/lighting/application/ReadKeyboardStatus.hpp"

#include <chrono>
#include <utility>

namespace ghub::lighting::application {

namespace {

constexpr auto leaseHolder = "lighting.status";
constexpr auto interfaceName = "hidpp:046d:407c";
constexpr std::chrono::seconds leaseDuration{10};

} // namespace

shared_kernel::Result<KeyboardStatus> ReadKeyboardStatus::execute() const
{
    auto interfaceId = device_access::PhysicalInterfaceId::create(interfaceName);
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
        .purpose = "ler estado do teclado por HID++",
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    // `lease` vive até o fim do escopo: a interface fica reservada durante a leitura e é liberada na saída.
    return source_->read();
}

} // namespace ghub::lighting::application
