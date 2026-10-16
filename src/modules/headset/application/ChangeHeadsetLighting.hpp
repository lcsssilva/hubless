#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/headset/application/HeadsetLightingWriter.hpp"

#include <utility>

namespace ghub::headset {

// Caso de uso (INC-018 R1): aplica cor/efeito de uma zona do G733 por ação do usuário. Fluxo: DeviceLease
// (holder "headset.lighting", 5 s; ocupado -> DeviceBusy) -> lease->verify() -> writer, uma vez, sem retentativa.
// Devolve o valor aplicado. Pode ser chamado de thread de trabalho (o writer bloqueia). Os colaboradores precisam
// sobreviver ao caso de uso.
class ChangeHeadsetLighting {
public:
    ChangeHeadsetLighting(HeadsetLightingWriter& writer, device_access::AccessRegistry& registry,
                          device_access::PhysicalInterfaceId interfaceId)
        : writer_(&writer), registry_(&registry), interfaceId_(std::move(interfaceId))
    {
    }

    [[nodiscard]] shared_kernel::Result<HeadsetLighting> apply(HeadsetLighting lighting) const;

private:
    HeadsetLightingWriter* writer_;           // não nulo
    device_access::AccessRegistry* registry_; // não nulo
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::headset
