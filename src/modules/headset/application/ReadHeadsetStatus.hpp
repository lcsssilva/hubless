#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/headset/application/HeadsetStatusSource.hpp"

namespace ghub::headset {

// Caso de uso (INC-015 R10/R12): lê o estado do G733 sob o DeviceLease "hidpp:046d:0afe" (dono
// "headset.status", prazo de 10 s). Lease ocupado -> DeviceBusy imediato com o dono, sem chamar a fonte. O lease é
// liberado ao fim, inclusive em erro. O registro e a fonte devem sobreviver ao caso de uso.
class ReadHeadsetStatus {
public:
    ReadHeadsetStatus(HeadsetStatusSource& source, device_access::AccessRegistry& registry)
        : source_(&source), registry_(&registry)
    {
    }

    [[nodiscard]] shared_kernel::Result<HeadsetStatus> execute() const;

private:
    HeadsetStatusSource* source_;             // não nulo; não possuído
    device_access::AccessRegistry* registry_; // não nulo; não possuído
};

} // namespace ghub::headset
