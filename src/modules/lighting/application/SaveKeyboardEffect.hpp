#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/lighting/application/KeyboardLighting.hpp"
#include "modules/lighting/domain/KeyboardEffect.hpp"
#include "modules/lighting/domain/KeyboardZone.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::lighting::application {

// Caso de uso: envia um efeito de hardware ao teclado e devolve o controle (testes D1/D2: nesta unidade o efeito não
// persiste; INC-008 parte D). Só por ação explícita do usuário. Ordem: processos externos -> lease -> abre -> toma o
// controle -> grava -> devolve o controle. Com openrgb, solaar, piper ou ratbagd em execução, ou com o lease ocupado,
// devolve DeviceBusy sem abrir o nó. Falha ao tomar o controle ou ao gravar: tenta devolver o controle uma vez e
// devolve o erro original. Nunca reenvia nada. Construir o caso de uso não consulta nada.
class SaveKeyboardEffect {
public:
    // `lighting`, `registry` e `probe` precisam sobreviver ao caso de uso.
    SaveKeyboardEffect(KeyboardLighting& lighting, device_access::AccessRegistry& registry,
                       device_access::ExternalProcessProbe& probe, device_access::PhysicalInterfaceId interfaceId);

    [[nodiscard]] shared_kernel::Result<void> save(domain::KeyboardZone zone,
                                                   const domain::KeyboardEffect& effect) const;

private:
    KeyboardLighting* lighting_;              // não nulo
    device_access::AccessRegistry* registry_; // não nulo
    device_access::ExternalProcessProbe* probe_;
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::lighting::application
