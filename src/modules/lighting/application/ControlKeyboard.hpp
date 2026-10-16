#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/lighting/application/KeyboardControl.hpp"
#include "shared_kernel/Result.hpp"

#include <functional>

namespace ghub::lighting::application {

// Caso de uso (INC-019 R2/R3/R6, ADR-0029): modo de jogo e troca de slot onboard do G915, uma operação por pedido do
// usuário. Ordem de cada operação: processos externos -> lease da interface do teclado -> abre a sessão -> executa;
// o lease é liberado ao fim, inclusive em erro. Com openrgb, solaar, piper ou ratbagd em execução, ou com o lease
// ocupado (ex.: modo ao vivo), devolve DeviceBusy sem abrir o nó. Nada é reenviado. Construir não consulta nada.
// `control`, `registry` e `probe` precisam sobreviver ao caso de uso.
class ControlKeyboard {
public:
    ControlKeyboard(KeyboardControl& control, device_access::AccessRegistry& registry,
                    device_access::ExternalProcessProbe& probe, device_access::PhysicalInterfaceId interfaceId);

    // fn3 + fn1 com as teclas (lista vazia reativa todas).
    [[nodiscard]] shared_kernel::Result<void> setGameMode(const domain::GameModeKeys& keys) const;
    // Só leitura: slots habilitados e o ativo.
    [[nodiscard]] shared_kernel::Result<domain::OnboardSlots> listSlots() const;
    // Lê o diretório e só troca se o slot estiver habilitado (senão InvalidArgument, sem enviar a troca).
    [[nodiscard]] shared_kernel::Result<void> selectSlot(domain::OnboardSlot slot) const;

private:
    // Probe -> lease -> open -> operação; o lease morre ao sair.
    template <typename Result>
    [[nodiscard]] Result run(const std::function<Result(KeyboardControlSession&)>& operation) const;

    KeyboardControl* control_;                // não nulo
    device_access::AccessRegistry* registry_; // não nulo
    device_access::ExternalProcessProbe* probe_;
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::lighting::application
