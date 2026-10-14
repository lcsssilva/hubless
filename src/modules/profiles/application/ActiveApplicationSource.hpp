#pragma once

#include "modules/profiles/domain/ApplicationId.hpp"
#include "shared_kernel/DomainError.hpp"
#include "shared_kernel/Result.hpp"

#include <functional>
#include <optional>

namespace ghub::profiles {

// Port: fonte do aplicativo em foco na sessão (ADR-0022, R6-R9). Só é iniciada com a troca automática ligada.
// start/stop são chamados na thread dona. Os callbacks podem ser chamados em qualquer thread; quem recebe deve
// reenfileirar para a thread dona. Nunca entrega título de janela; classes inválidas são descartadas pela fonte.
class ActiveApplicationSource {
public:
    using ActivationListener = std::function<void(ApplicationId)>;
    // nullopt = fonte voltou a funcionar (ex.: script recarregado); erro = ficou indisponível, com o motivo.
    using StatusListener = std::function<void(std::optional<shared_kernel::DomainError>)>;

    virtual ~ActiveApplicationSource() = default;

    // Começa a observar. Já iniciada -> sucesso sem efeito. Erro BackendUnavailable com o motivo (sem KWin, sem
    // XDG_RUNTIME_DIR, loadScript recusado, prazo esgotado); nesse caso nada fica carregado.
    [[nodiscard]] virtual shared_kernel::Result<void> start(ActivationListener onActivated,
                                                            StatusListener onStatus) = 0;

    // Para de observar e descarrega o que carregou. Parada -> sem efeito. Nunca lança.
    virtual void stop() noexcept = 0;

protected:
    ActiveApplicationSource() = default;
    ActiveApplicationSource(const ActiveApplicationSource&) = default;
    ActiveApplicationSource& operator=(const ActiveApplicationSource&) = default;
    ActiveApplicationSource(ActiveApplicationSource&&) = default;
    ActiveApplicationSource& operator=(ActiveApplicationSource&&) = default;
};

} // namespace ghub::profiles
