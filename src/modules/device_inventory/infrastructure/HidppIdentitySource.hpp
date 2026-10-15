#pragma once

#include "modules/device_inventory/application/IdentitySource.hpp"
#include "platform/hidpp/HidReportIo.hpp"
#include "platform/hidpp/HidppNodeLocator.hpp"
#include "shared_kernel/Clock.hpp"

#include <filesystem>
#include <functional>
#include <memory>

namespace ghub::device_inventory {

// Abre o nó hidraw e entrega o canal de relatórios. Injetável para testar com um HidReportIo falso.
using HidReportIoOpener =
    std::function<shared_kernel::Result<std::unique_ptr<platform::hidpp::HidReportIo>>(const std::filesystem::path&)>;

// Abridor de produção: HidrawDevice::open (O_RDWR | O_CLOEXEC). Abrir não envia nada.
[[nodiscard]] HidReportIoOpener hidrawOpener();

// Adaptador do port IdentitySource por HID++ 2.0 sobre hidraw (ADR-0024, INC-015). Só leituras, todas dentro da lista
// fechada de InventoryIdentityPolicy. A cada read() localiza o nó, abre o hidraw, lê e fecha (o nó só fica aberto
// durante a leitura). Sem estado entre leituras: pode ser chamado de várias threads. O unit ID da 0x0003 fn0 é
// descartado aqui (R3). `clock` precisa sobreviver à fonte.
class HidppIdentitySource final : public IdentitySource {
public:
    HidppIdentitySource(platform::hidpp::HidppNodeLocator locator, const shared_kernel::Clock& clock,
                        HidReportIoOpener opener = hidrawOpener());

    [[nodiscard]] shared_kernel::Result<DeviceIdentity> read(IdentityTarget target) override;

private:
    platform::hidpp::HidppNodeLocator locator_;
    const shared_kernel::Clock* clock_; // não nulo
    HidReportIoOpener opener_;
};

} // namespace ghub::device_inventory
