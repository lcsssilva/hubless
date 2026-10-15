#pragma once

#include "modules/device_inventory/application/IdentityTarget.hpp"
#include "modules/device_inventory/domain/DeviceIdentity.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::device_inventory {

// Port: lê a identidade de um dispositivo (nome, tipo, firmware, mapa de features). Só leitura. O chamador
// (ReadDeviceIdentity) já segura o DeviceLease da interface; a fonte não decide sobre acesso exclusivo.
// Pode ser chamada de threads de trabalho para alvos diferentes ao mesmo tempo: não guarda estado entre leituras.
class IdentitySource {
public:
    virtual ~IdentitySource() = default;

    [[nodiscard]] virtual shared_kernel::Result<DeviceIdentity> read(IdentityTarget target) = 0;

protected:
    IdentitySource() = default;
    IdentitySource(const IdentitySource&) = default;
    IdentitySource& operator=(const IdentitySource&) = default;
    IdentitySource(IdentitySource&&) = default;
    IdentitySource& operator=(IdentitySource&&) = default;
};

} // namespace ghub::device_inventory
