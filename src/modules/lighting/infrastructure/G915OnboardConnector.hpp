#pragma once

#include "modules/onboard_memory/api/OnboardMemory.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <utility>

namespace ghub::lighting::infrastructure {

// Abre o hidraw do receptor do G915 só durante a operação e entrega a memória onboard HID++ do módulo onboard_memory
// com a G915OnboardPolicy (INC-019 R4/R5, ADR-0029). Fecha ao destruir a memória. Abrir não envia nada. O relógio
// precisa sobreviver ao conector e às memórias abertas por ele.
class G915OnboardConnector final : public onboard_memory::OnboardMemoryConnector {
public:
    G915OnboardConnector(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock)
        : locator_(std::move(locator)), clock_(&clock)
    {
    }

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<onboard_memory::OnboardMemory>> open() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
};

} // namespace ghub::lighting::infrastructure
