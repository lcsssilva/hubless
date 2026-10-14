#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/pointer/application/PointerBackend.hpp"
#include "modules/pointer/application/PointerStatus.hpp"

namespace ghub::pointer {

// Caso de uso: lê a configuração do G703 pelo backend (ADR-0011 §3). Antes de ler, recusa se um
// software externo que disputa o dispositivo estiver ativo (openrgb, solaar, headsetcontrol) e segura
// o DeviceLease da interface durante a leitura. Piper e ratbagd não bloqueiam.
class ReadPointerConfiguration {
public:
    // Os colaboradores precisam sobreviver ao caso de uso.
    ReadPointerConfiguration(PointerBackend& backend, device_access::ExternalProcessProbe& probe,
                             device_access::AccessRegistry& registry, device_access::PhysicalInterfaceId interfaceId)
        : backend_(&backend), probe_(&probe), registry_(&registry), interfaceId_(std::move(interfaceId))
    {
    }

    [[nodiscard]] PointerStatus execute() const;

private:
    PointerBackend* backend_;                    // não nulo
    device_access::ExternalProcessProbe* probe_; // não nulo
    device_access::AccessRegistry* registry_;    // não nulo
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::pointer
