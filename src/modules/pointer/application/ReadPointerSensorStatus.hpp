#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/pointer/application/PointerSensorSource.hpp"

namespace ghub::pointer {

// Caso de uso (INC-015 R10/R12): lê o estado do sensor do G703 sob o DeviceLease "hidpp:046d:c539" (dono
// "pointer.status", prazo de 10 s). Lease ocupado -> DeviceBusy imediato com o dono, sem chamar a fonte. O lease é
// liberado ao fim, inclusive em erro. O registro e a fonte devem sobreviver ao caso de uso.
class ReadPointerSensorStatus {
public:
    ReadPointerSensorStatus(PointerSensorSource& source, device_access::AccessRegistry& registry)
        : source_(&source), registry_(&registry)
    {
    }

    [[nodiscard]] shared_kernel::Result<PointerSensorStatus> execute() const;

private:
    PointerSensorSource* source_;             // não nulo; não possuído
    device_access::AccessRegistry* registry_; // não nulo; não possuído
};

} // namespace ghub::pointer
