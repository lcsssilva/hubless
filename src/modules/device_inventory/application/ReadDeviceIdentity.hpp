#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_inventory/application/IdentitySource.hpp"

namespace ghub::device_inventory {

// Caso de uso (INC-015 R1/R4): lê a identidade de um dispositivo sob o DeviceLease da interface dele
// ("hidpp:046d:407c" G915, "hidpp:046d:c539" G703, "hidpp:046d:0afe" G733; dono "inventory.identity", prazo de 10 s).
// Lease ocupado -> DeviceBusy imediato, sem chamar a fonte. O lease é liberado ao fim, inclusive em erro.
// O registro e a fonte devem sobreviver ao caso de uso; a fonte precisa aceitar chamadas de threads de trabalho.
class ReadDeviceIdentity {
public:
    ReadDeviceIdentity(IdentitySource& source, device_access::AccessRegistry& registry)
        : source_(&source), registry_(&registry)
    {
    }

    [[nodiscard]] shared_kernel::Result<DeviceIdentity> execute(IdentityTarget target) const;

private:
    IdentitySource* source_;                  // não nulo; não possuído
    device_access::AccessRegistry* registry_; // não nulo; não possuído
};

} // namespace ghub::device_inventory
