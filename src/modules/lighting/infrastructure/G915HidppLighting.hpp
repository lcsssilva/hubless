#pragma once

#include "modules/lighting/application/KeyboardLighting.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "platform/hidpp/HidppTrace.hpp"
#include "shared_kernel/Clock.hpp"

namespace ghub::lighting::infrastructure {

// Adaptador do port KeyboardLighting para o G915 sem fio (ADR-0013): localiza o nó do receptor pelo sysfs, abre o
// hidraw e entrega uma G915HidppSession dona do canal. Abrir não envia nada. `clock` precisa sobreviver às sessões.
class G915HidppLighting final : public application::KeyboardLighting {
public:
    // `trace` (opcional) é repassado a cada canal HID++ criado em open().
    G915HidppLighting(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                      platform::hidpp::HidppTraceSink trace = {})
        : locator_(std::move(locator)), clock_(&clock), trace_(std::move(trace))
    {
    }

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<application::KeyboardLightingSession>> open() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    platform::hidpp::HidppTraceSink trace_;
};

} // namespace ghub::lighting::infrastructure
