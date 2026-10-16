#pragma once

#include "modules/lighting/application/KeyboardStatusSource.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::lighting::infrastructure {

// Abre o nó hidraw e entrega o canal de relatórios. Injetável para testar com um HidReportIo falso.
using StatusIoOpener =
    std::function<shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>>(const std::filesystem::path&)>;

// Abridor de produção: HidrawDevice::open (O_RDWR | O_CLOEXEC). Abrir não envia nada.
[[nodiscard]] StatusIoOpener statusHidrawOpener();

// Adaptador do port KeyboardStatusSource por HID++ 2.0 (ADR-0025, INC-015 parte 2): 0x8040 fn0/fn1 e 0x8060 fn0/fn1
// do G915, todos sem parâmetros e dentro de LightingHidppPolicy. Separado do modo ao vivo (G915HidppLighting). A cada
// read() localiza o nó, abre o hidraw, lê e fecha. Prazo de 2 s por pedido; sem reenvio. Sem estado entre leituras.
// `clock` precisa sobreviver à fonte.
class HidppKeyboardStatusSource final : public application::KeyboardStatusSource {
public:
    HidppKeyboardStatusSource(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                              StatusIoOpener opener = statusHidrawOpener());

    [[nodiscard]] shared_kernel::Result<KeyboardStatus> read() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    StatusIoOpener opener_;
};

} // namespace ghub::lighting::infrastructure
