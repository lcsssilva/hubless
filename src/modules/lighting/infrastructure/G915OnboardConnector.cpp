#include "modules/lighting/infrastructure/G915OnboardConnector.hpp"

#include "modules/lighting/infrastructure/G915OnboardPolicy.hpp"
#include "modules/onboard_memory/api/HidppOnboardMemory.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <memory>

namespace ghub::lighting::infrastructure {

shared_kernel::Result<std::unique_ptr<onboard_memory::OnboardMemory>> G915OnboardConnector::open()
{
    const auto node = locator_.locateG915();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto device = platform::hidpp::HidrawDevice::open(node->devicePath);
    if (!device.has_value()) {
        return std::unexpected(device.error());
    }
    return onboard_memory::makeHidppOnboardMemory(std::move(*device), *clock_, node->deviceIndex,
                                                  std::make_unique<G915OnboardPolicy>());
}

} // namespace ghub::lighting::infrastructure
