#pragma once

#include "modules/battery/domain/BatteryReading.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::battery {

// Abre o nó hidraw e entrega o canal de relatórios. Injetável para testar com um HidReportIo falso.
using BatteryIoOpener =
    std::function<shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>>(const std::filesystem::path&)>;

// Abridor de produção: HidrawDevice::open (O_RDWR | O_CLOEXEC). Abrir não envia nada.
[[nodiscard]] BatteryIoOpener batteryHidrawOpener();

// Leitor bloqueante da bateria do G733 por HID++ 2.0 próprio (ADR-0026, INC-016 A1/A2): 0x1f20 fn0 (índice de
// dispositivo 0xFF), sem parâmetros e dentro de G733BatteryPolicy. A cada read() localiza o nó, abre o hidraw, lê e
// fecha; prazo de 2 s por pedido, sem reenvio. Erro 0x05 = "headset desligado ou fora de alcance". Não decide sobre
// acesso exclusivo: o chamador segura o DeviceLease "hidpp:046d:0afe". `clock` precisa sobreviver ao leitor.
class HidppBatteryReader {
public:
    HidppBatteryReader(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                       BatteryIoOpener opener = batteryHidrawOpener());

    [[nodiscard]] shared_kernel::Result<BatteryReading> read();

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    BatteryIoOpener opener_;
};

} // namespace ghub::battery
