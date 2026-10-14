#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "modules/lighting/application/KeyboardLighting.hpp"
#include "modules/lighting/domain/KeyColorMap.hpp"
#include "shared_kernel/Result.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace ghub::lighting::application {

// Sessão ao vivo com o teclado (INC-008 R5, R6, R9, R11). Objeto RAII: fechar (end() ou destrutor) devolve o
// controle ao firmware uma única vez. Nunca escreve sem um DeviceLease válido: cada operação renova o lease
// (libera e pede de novo, prazo de 15 s); lease revogado (suspensão, remoção) faz a pintura falhar sem escrever.
// Qualquer falha encerra a sessão: tenta uma vez devolver o controle e não reenvia a operação que falhou.
// R14: applyEffect envia um efeito de hardware (prévia) com as mesmas garantias da pintura; o efeito só vale enquanto
// a sessão segura o controle e some quando ela é encerrada (o efeito do firmware volta, testes D1/D2 de 2026-10-14).
// O SetEffect vai com persistência 01 (ver KeyboardLightingSession::setEffect): não é provado que nada seja gravado.
// Não é segura para uso concorrente.
class LiveKeyboardSession {
public:
    LiveKeyboardSession(const LiveKeyboardSession&) = delete;
    LiveKeyboardSession& operator=(const LiveKeyboardSession&) = delete;
    LiveKeyboardSession(LiveKeyboardSession&&) = delete;
    LiveKeyboardSession& operator=(LiveKeyboardSession&&) = delete;
    ~LiveKeyboardSession();

    [[nodiscard]] shared_kernel::Result<void> paint(const domain::KeyColorMap& colors);
    [[nodiscard]] shared_kernel::Result<void> applyEffect(domain::KeyboardZone zone,
                                                          const domain::KeyboardEffect& effect);
    // Devolve o controle ao firmware. Chamadas seguintes não enviam nada.
    [[nodiscard]] shared_kernel::Result<void> end();
    // R15: true quando o firmware retomou o controle (repouso do teclado). Nesse caso a sessão é descartada SEM enviar
    // nada (o firmware já tem o controle) e o lease é liberado. Só lê, não renova o lease. Sessão fechada -> false.
    // Erro de leitura -> o erro, e a sessão continua aberta.
    [[nodiscard]] shared_kernel::Result<bool> checkControl();
    [[nodiscard]] bool isOpen() const noexcept { return session_ != nullptr; }

private:
    friend class StartLiveKeyboardSession;

    LiveKeyboardSession(std::unique_ptr<KeyboardLightingSession> session, device_access::DeviceLease lease,
                        device_access::AccessRegistry& registry, device_access::LeaseRequest renewal);

    // Sessão aberta -> renova o lease -> executa; qualquer falha fecha a sessão e devolve o erro original.
    [[nodiscard]] shared_kernel::Result<void>
    guarded(const std::function<shared_kernel::Result<void>(KeyboardLightingSession&)>& operation);
    [[nodiscard]] shared_kernel::Result<void> renewLease();
    // Devolve o controle (com lease) e fecha a sessão; nunca repete.
    [[nodiscard]] shared_kernel::Result<void> close();

    std::unique_ptr<KeyboardLightingSession> session_; // nulo depois de fechada
    std::optional<device_access::DeviceLease> lease_;
    device_access::AccessRegistry* registry_; // não nulo
    device_access::LeaseRequest renewal_;
};

// Caso de uso: abre a sessão ao vivo por ação do usuário. Ordem: processos externos -> lease -> abre -> toma o
// controle. Com openrgb, solaar, piper ou ratbagd em execução, ou com o lease ocupado, devolve DeviceBusy sem
// abrir o nó. Construir o caso de uso não consulta nada.
class StartLiveKeyboardSession {
public:
    // `lighting`, `registry` e `probe` precisam sobreviver ao caso de uso e às sessões que ele criar.
    StartLiveKeyboardSession(KeyboardLighting& lighting, device_access::AccessRegistry& registry,
                             device_access::ExternalProcessProbe& probe,
                             device_access::PhysicalInterfaceId interfaceId);

    [[nodiscard]] shared_kernel::Result<std::unique_ptr<LiveKeyboardSession>> start() const;

private:
    KeyboardLighting* lighting_;              // não nulo
    device_access::AccessRegistry* registry_; // não nulo
    device_access::ExternalProcessProbe* probe_;
    device_access::PhysicalInterfaceId interfaceId_;
};

} // namespace ghub::lighting::application
