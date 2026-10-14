#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/headset/application/HeadsetSettingsWriter.hpp"

namespace ghub::headset {

// Caso de uso: aplica um ajuste (sidetone ou luzes) no headset por ação do usuário (ADR-0015 decisão 4).
// Fluxo: sonda de processos externos (qualquer dono conhecido -> DeviceBusy "em uso por <nome>") -> DeviceLease
// (holder "headset.headsetcontrol", 15 s; ocupado -> DeviceBusy) -> lease->verify() -> writer, uma vez, sem
// retentativa. Devolve o valor aplicado. O lease vive só durante a operação. Pode ser chamado de thread de
// trabalho (o writer bloqueia).
class ChangeHeadsetSetting {
public:
    // Os colaboradores precisam sobreviver ao caso de uso.
    ChangeHeadsetSetting(HeadsetSettingsWriter& writer, device_access::ExternalProcessProbe& probe,
                         device_access::AccessRegistry& registry, device_access::PhysicalInterfaceId interfaceId)
        : writer_(&writer), probe_(&probe), registry_(&registry), interfaceId_(std::move(interfaceId))
    {
    }

    [[nodiscard]] shared_kernel::Result<SidetoneLevel> setSidetone(SidetoneLevel level) const;
    [[nodiscard]] shared_kernel::Result<HeadsetLights> setLights(HeadsetLights lights) const;

private:
    HeadsetSettingsWriter* writer_;              // não nulo
    device_access::ExternalProcessProbe* probe_; // não nulo
    device_access::AccessRegistry* registry_;    // não nulo
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::headset
