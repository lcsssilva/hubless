#include "modules/pointer/infrastructure/OnboardPointerBackend.hpp"

#include "modules/pointer/domain/OnboardPointerProfile.hpp"

#include <utility>

namespace ghub::pointer::infrastructure {

namespace {

constexpr device_inventory::UsbId kG703Receiver{.vendorId = 0x046d, .productId = 0x4086};

} // namespace

shared_kernel::Result<std::vector<DiscoveredPointer>> OnboardPointerBackend::readDevices()
{
    auto memory = connector_->open();
    if (!memory.has_value()) {
        return std::unexpected(memory.error());
    }
    const auto active = onboard_memory::inspectActiveSector(**memory);
    if (!active.has_value()) {
        return std::unexpected(active.error());
    }
    std::vector<DiscoveredPointer> found;
    found.push_back({
        .usbId = kG703Receiver,
        .name = "G703",
        .configuration = pointerConfigurationFromOnboardSector(active->number, active->sector.bytes()),
    });
    return found;
}

} // namespace ghub::pointer::infrastructure
