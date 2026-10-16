#pragma once

#include "modules/lighting/application/KeyboardControl.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "platform/hidpp/HidppTrace.hpp"
#include "shared_kernel/Clock.hpp"

namespace ghub::lighting::infrastructure {

// Adaptador do port KeyboardControl para o G915 sem fio (INC-019, ADR-0029): localiza o nó do receptor pelo sysfs,
// abre o hidraw e entrega uma G915HidppControlSession dona do canal (política LightingHidppPolicy). Abrir não envia
// nada. `clock` precisa sobreviver às sessões.
class G915HidppControl final : public application::KeyboardControl {
public:
    G915HidppControl(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                     platform::hidpp::HidppTraceSink trace = {})
        : locator_(std::move(locator)), clock_(&clock), trace_(std::move(trace))
    {
    }

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<application::KeyboardControlSession>> open() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    platform::hidpp::HidppTraceSink trace_;
};

} // namespace ghub::lighting::infrastructure
