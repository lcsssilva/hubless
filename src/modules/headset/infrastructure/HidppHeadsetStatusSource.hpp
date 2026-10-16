#pragma once

#include "modules/headset/application/HeadsetStatusSource.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::headset::infrastructure {

// Abre o nó hidraw e entrega o canal de relatórios. Injetável para testar com um HidReportIo falso.
using HeadsetIoOpener =
    std::function<shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>>(const std::filesystem::path&)>;

// Abridor de produção: HidrawDevice::open (O_RDWR | O_CLOEXEC). Abrir não envia nada.
[[nodiscard]] HeadsetIoOpener headsetHidrawOpener();

// Adaptador do port HeadsetStatusSource por HID++ 2.0 (ADR-0025, INC-015 parte 2): 0x8310 fn0 e 0x8300 fn0 do G733
// (índice de dispositivo 0xFF), sem parâmetros e dentro de G733StatusPolicy. A cada read() localiza o nó
// (HidppNodeLocator::locateG733), abre o hidraw, lê e fecha. Prazo de 2 s por pedido; sem reenvio. Erro 0x05 do
// headset = desligado ou fora de alcance. Sem estado entre leituras. `clock` precisa sobreviver à fonte.
class HidppHeadsetStatusSource final : public HeadsetStatusSource {
public:
    HidppHeadsetStatusSource(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                             HeadsetIoOpener opener = headsetHidrawOpener());

    [[nodiscard]] shared_kernel::Result<HeadsetStatus> read() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    HeadsetIoOpener opener_;
};

} // namespace ghub::headset::infrastructure
