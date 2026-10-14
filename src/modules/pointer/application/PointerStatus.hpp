#pragma once

#include "modules/device_inventory/api/DeviceInventory.hpp"
#include "modules/pointer/domain/PointerConfiguration.hpp"
#include "shared_kernel/DomainError.hpp"

#include <variant>

namespace ghub::pointer {

// Nenhum G703 entre os dispositivos do serviço.
struct NotDetected {};

// A leitura não aconteceu ou falhou (ocupado, serviço ausente, dados inesperados).
struct Unavailable {
    shared_kernel::DomainError error;
};

// G703 lido, com o caminho de conexão em que foi enumerado.
struct Ready {
    PointerConfiguration configuration;
    device_inventory::ConnectionPath path;
};

using PointerStatus = std::variant<NotDetected, Unavailable, Ready>;

} // namespace ghub::pointer
