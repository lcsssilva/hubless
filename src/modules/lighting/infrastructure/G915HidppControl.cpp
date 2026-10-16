#include "modules/lighting/infrastructure/G915HidppControl.hpp"

#include "modules/lighting/infrastructure/G915HidppControlSession.hpp"
#include "modules/lighting/infrastructure/LightingHidppPolicy.hpp"
#include "platform/hidpp/HidppChannel.hpp"
#include "platform/hidpp/HidrawDevice.hpp"

#include <utility>

namespace ghub::lighting::infrastructure {

namespace {

// Sessão dona do canal: o canal (e o hidraw) fecham depois da sessão que os usa.
class OwningControlSession final : public application::KeyboardControlSession {
public:
    OwningControlSession(std::unique_ptr<platform::hidpp::HidReportIo> io, const shared_kernel::Clock& clock,
                         platform::hidpp::HidppTraceSink trace, std::uint8_t deviceIndex)
        : channel_(std::make_unique<platform::hidpp::HidppChannel>(
              std::move(io), clock, policy_, platform::hidpp::HidppChannel::defaultTimeout, std::move(trace))),
          session_(*channel_, deviceIndex)
    {
    }

    shared_kernel::Result<void> setGameMode(const domain::GameModeKeys& keys) override
    {
        return session_.setGameMode(keys);
    }
    shared_kernel::Result<domain::OnboardSlots> readSlots() override { return session_.readSlots(); }
    shared_kernel::Result<void> selectSlot(domain::OnboardSlot slot) override { return session_.selectSlot(slot); }

private:
    LightingHidppPolicy policy_; // antes do canal: a política precisa viver mais que ele
    std::unique_ptr<platform::hidpp::HidppChannel> channel_;
    G915HidppControlSession session_;
};

} // namespace

shared_kernel::Result<std::unique_ptr<application::KeyboardControlSession>> G915HidppControl::open()
{
    const auto node = locator_.locateG915();
    if (!node.has_value()) {
        return std::unexpected(node.error());
    }
    auto device = platform::hidpp::HidrawDevice::open(node->devicePath);
    if (!device.has_value()) {
        return std::unexpected(device.error());
    }
    return std::make_unique<OwningControlSession>(std::move(*device), *clock_, trace_, node->deviceIndex);
}

} // namespace ghub::lighting::infrastructure
