#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/lighting/application/KeyboardStatusSource.hpp"

namespace ghub::lighting::application {

// Caso de uso (INC-015 R10/R12): lê o estado do G915 sob o DeviceLease "hidpp:046d:407c" (dono "lighting.status",
// prazo de 10 s). Lease ocupado (ex.: modo ao vivo) -> DeviceBusy imediato com o dono, sem chamar a fonte.
// O lease é liberado ao fim, inclusive em erro. O registro e a fonte devem sobreviver ao caso de uso.
class ReadKeyboardStatus {
public:
    ReadKeyboardStatus(KeyboardStatusSource& source, device_access::AccessRegistry& registry)
        : source_(&source), registry_(&registry)
    {
    }

    [[nodiscard]] shared_kernel::Result<KeyboardStatus> execute() const;

private:
    KeyboardStatusSource* source_;            // não nulo; não possuído
    device_access::AccessRegistry* registry_; // não nulo; não possuído
};

} // namespace ghub::lighting::application
