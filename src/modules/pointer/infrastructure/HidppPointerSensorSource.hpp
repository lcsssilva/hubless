#pragma once

#include "modules/pointer/application/PointerSensorSource.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::pointer::infrastructure {

// Abre o nó hidraw e entrega o canal de relatórios. Injetável para testar com um HidReportIo falso.
using SensorIoOpener =
    std::function<shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>>(const std::filesystem::path&)>;

// Abridor de produção: HidrawDevice::open (O_RDWR | O_CLOEXEC). Abrir não envia nada.
[[nodiscard]] SensorIoOpener sensorHidrawOpener();

// Adaptador do port PointerSensorSource por HID++ 2.0 (ADR-0025, INC-015 parte 2): 0x2201 fn1/fn2 (sensor 0) e 0x8060
// fn0/fn1 do G703, dentro de G703OnboardPolicy. Separado da memória onboard (HidppOnboardMemory). A cada read()
// localiza o nó (HidppNodeLocator::locateG703), abre o hidraw, lê e fecha. Prazo de 2 s por pedido; sem reenvio.
// Sem estado entre leituras. `clock` precisa sobreviver à fonte.
class HidppPointerSensorSource final : public PointerSensorSource {
public:
    HidppPointerSensorSource(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                             SensorIoOpener opener = sensorHidrawOpener());

    [[nodiscard]] shared_kernel::Result<PointerSensorStatus> read() override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    SensorIoOpener opener_;
};

} // namespace ghub::pointer::infrastructure
