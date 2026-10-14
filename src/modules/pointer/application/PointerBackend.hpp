#pragma once

#include "modules/device_inventory/api/DeviceInventory.hpp"
#include "modules/pointer/domain/PointerConfiguration.hpp"
#include "shared_kernel/Result.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ghub::pointer {

// Um dispositivo relatado pelo serviço de configuração (ratbagd). O USB ID só existe para dispositivos USB.
// A configuração falha por dispositivo (dados inesperados) sem invalidar os demais.
struct DiscoveredPointer {
    std::optional<device_inventory::UsbId> usbId;
    std::string name;
    shared_kernel::Result<PointerConfiguration> configuration;
};

// Port: lê, de forma síncrona e somente leitura, todos os dispositivos que o serviço de configuração
// conhece. Erro global: o serviço está indisponível ou incompatível.
class PointerBackend {
public:
    virtual ~PointerBackend() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::vector<DiscoveredPointer>> readDevices() = 0;

protected:
    PointerBackend() = default;
    PointerBackend(const PointerBackend&) = default;
    PointerBackend& operator=(const PointerBackend&) = default;
    PointerBackend(PointerBackend&&) = default;
    PointerBackend& operator=(PointerBackend&&) = default;
};

} // namespace ghub::pointer
