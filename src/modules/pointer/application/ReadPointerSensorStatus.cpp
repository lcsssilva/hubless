#include "modules/pointer/application/ReadPointerSensorStatus.hpp"

#include <chrono>
#include <utility>

namespace ghub::pointer {

namespace {

constexpr auto leaseHolder = "pointer.status";
constexpr auto interfaceName = "hidpp:046d:c539";
constexpr std::chrono::seconds leaseDuration{10};

} // namespace

shared_kernel::Result<PointerSensorStatus> ReadPointerSensorStatus::execute() const
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
        .purpose = "ler estado do sensor por HID++",
        .maxDuration = leaseDuration,
    });
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }
    // `lease` vive até o fim do escopo: a interface fica reservada durante a leitura e é liberada na saída.
    return source_->read();
}

} // namespace ghub::pointer
