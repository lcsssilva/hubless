#pragma once

#include "modules/profiles/application/ActiveApplicationSource.hpp"

#include <QDBusConnection>
#include <QString>

#include <chrono>
#include <memory>

namespace ghub::profiles {

// Adaptador do port ActiveApplicationSource para o KWin (ADR-0022): grava um script constante em
// `<runtimeDir>/ghub-linux/active-window.js`, carrega-o no KWin por org.kde.kwin.Scripting e recebe, num serviço
// D-Bus de sessão próprio, a classe do aplicativo em foco. Nunca lê nem recebe o título da janela. Os callbacks
// são chamados na thread dona da conexão (a que processa eventos). Não copiável nem movível.
class KWinActiveApplicationSource final : public ActiveApplicationSource {
public:
    static constexpr std::chrono::milliseconds defaultDeadline{2000};

    // `connection`: barramento de sessão (injetado; produção passa QDBusConnection::sessionBus()).
    // `runtimeDir`: produção passa $XDG_RUNTIME_DIR. `deadline`: prazo de cada chamada ao KWin.
    KWinActiveApplicationSource(QDBusConnection connection, QString runtimeDir,
                                std::chrono::milliseconds deadline = defaultDeadline);
    ~KWinActiveApplicationSource() override;

    KWinActiveApplicationSource(const KWinActiveApplicationSource&) = delete;
    KWinActiveApplicationSource& operator=(const KWinActiveApplicationSource&) = delete;
    KWinActiveApplicationSource(KWinActiveApplicationSource&&) = delete;
    KWinActiveApplicationSource& operator=(KWinActiveApplicationSource&&) = delete;

    [[nodiscard]] shared_kernel::Result<void> start(ActivationListener onActivated, StatusListener onStatus) override;
    void stop() noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ghub::profiles
