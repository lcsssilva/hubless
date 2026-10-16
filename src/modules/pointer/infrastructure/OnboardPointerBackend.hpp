#pragma once

#include "modules/onboard_memory/api/OnboardMemory.hpp"
#include "modules/pointer/application/PointerBackend.hpp"

namespace ghub::pointer::infrastructure {

// Adaptador do port PointerBackend que lê a configuração do G703 do setor onboard ativo, por HID++ próprio, sem
// ratbagd (INC-016 B2, ADR-0027): abre o nó, inspeciona o perfil ativo (0x8100 fn4, a partir de 1) e interpreta o
// setor. Só lê: describe/modo/perfil atual/leitura de setores; nunca escreve. Não adquire o DeviceLease (quem chama,
// ReadPointerConfiguration, já o segura). O conector precisa sobreviver ao adaptador.
class OnboardPointerBackend final : public PointerBackend {
public:
    explicit OnboardPointerBackend(onboard_memory::OnboardMemoryConnector& connector) : connector_(&connector) {}

    [[nodiscard]] shared_kernel::Result<std::vector<DiscoveredPointer>> readDevices() override;

private:
    onboard_memory::OnboardMemoryConnector* connector_; // não nulo
};

} // namespace ghub::pointer::infrastructure
