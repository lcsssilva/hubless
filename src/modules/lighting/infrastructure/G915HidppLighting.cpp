#include "modules/lighting/infrastructure/G915HidppLighting.hpp"

#include "modules/lighting/infrastructure/G915HidppSession.hpp"
#include "modules/lighting/infrastructure/HidppChannel.hpp"
#include "modules/lighting/infrastructure/HidrawDevice.hpp"

#include <utility>

namespace ghub::lighting::infrastructure {

namespace {

// Sessão dona do canal: o canal (e o hidraw) fecham depois da sessão que os usa.
class OwningG915Session final : public application::KeyboardLightingSession {
public:
    OwningG915Session(std::unique_ptr<HidppChannel> channel, std::uint8_t deviceIndex)
        : channel_(std::move(channel)), session_(*channel_, deviceIndex)
    {
    }

    shared_kernel::Result<void> beginLive() override { return session_.beginLive(); }
    shared_kernel::Result<void> paint(const domain::KeyColorMap& colors) override { return session_.paint(colors); }
    shared_kernel::Result<void> setEffect(domain::KeyboardZone zone, const domain::KeyboardEffect& effect) override
    {
        return session_.setEffect(zone, effect);
    }
    shared_kernel::Result<void> endLive() override { return session_.endLive(); }
    shared_kernel::Result<bool> controlLost() override { return session_.controlLost(); }

private:
    std::unique_ptr<HidppChannel> channel_;
    G915HidppSession session_;
};

} // namespace

shared_kernel::Result<std::unique_ptr<application::KeyboardLightingSession>> G915HidppLighting::open()
{
    const auto node = locator_.locateG915();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto device = HidrawDevice::open(node->devicePath);
    if (!device.has_value()) {
        return std::unexpected(device.error());
    }
    auto channel = std::make_unique<HidppChannel>(std::move(*device), *clock_, HidppChannel::defaultTimeout, trace_);
    return std::make_unique<OwningG915Session>(std::move(channel), node->deviceIndex);
}

} // namespace ghub::lighting::infrastructure
