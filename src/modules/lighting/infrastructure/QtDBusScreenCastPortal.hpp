#pragma once

#include "modules/lighting/infrastructure/ScreenCastPortal.hpp"

#include <QDBusConnection>
#include <QString>
#include <QVariantMap>

#include <chrono>

namespace ghub::lighting::infrastructure {

// Adaptador do portal org.freedesktop.portal.ScreenCast por QtDBus (INC-021 R4, ADR-0030). Fonte da interface:
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html (versão 6 da interface,
// verificada em 2026-10-16), Request e Session nas páginas vizinhas. Cada chamada assina o sinal Response do objeto de
// requisição ANTES de chamar (caminho previsto pelo handle_token), espera num QEventLoop local (por isso pode rodar em
// qualquer QThread, inclusive de trabalho) e consulta `cancelled` a cada 100 ms. Só usa monitor, um único, cursor
// oculto, sem persist_mode nem restore_token; o consentimento é do diálogo do portal a cada sessão. Nunca registra
// handles, nós nem descritores em log.
class QtDBusScreenCastPortal final : public ScreenCastPortalApi {
public:
    static constexpr std::chrono::milliseconds defaultCallDeadline{10'000};
    static constexpr std::chrono::milliseconds defaultUserDeadline{120'000};

    // `connection`: barramento de sessão (produção passa QDBusConnection::sessionBus()). `userDeadline`: quanto esperar
    // o usuário responder ao diálogo do Start.
    explicit QtDBusScreenCastPortal(QDBusConnection connection,
                                    std::chrono::milliseconds callDeadline = defaultCallDeadline,
                                    std::chrono::milliseconds userDeadline = defaultUserDeadline);

    [[nodiscard]] shared_kernel::Result<std::string> createSession(const std::atomic<bool>& cancelled) override;
    [[nodiscard]] shared_kernel::Result<void> selectSources(const std::string& session,
                                                            const std::atomic<bool>& cancelled) override;
    [[nodiscard]] shared_kernel::Result<std::uint32_t> start(const std::string& session,
                                                             const std::atomic<bool>& cancelled) override;
    [[nodiscard]] shared_kernel::Result<ScreenFd> openPipeWireRemote(const std::string& session) override;
    void closeSession(const std::string& session) noexcept override;

    // Partes puras (testáveis sem barramento).
    // ":1.42" -> "1_42".
    [[nodiscard]] static QString senderToken(const QString& uniqueName);
    [[nodiscard]] static QString requestPath(const QString& uniqueName, const QString& token);
    // "ghub_" + 64 bits aleatórios em hexadecimal.
    [[nodiscard]] static QString newToken();
    // Nó PipeWire do primeiro stream de `streams` (a(ua{sv})) nos resultados do Start.
    [[nodiscard]] static shared_kernel::Result<std::uint32_t> firstStreamNode(const QVariantMap& results);

private:
    // Chama `method`, espera o Response do pedido e devolve seus resultados (code 0) ou o erro.
    [[nodiscard]] shared_kernel::Result<QVariantMap> callAndAwait(const QString& method, const QVariantList& leading,
                                                                  QVariantMap options, std::chrono::milliseconds wait,
                                                                  const std::atomic<bool>& cancelled);

    QDBusConnection connection_;
    std::chrono::milliseconds callDeadline_;
    std::chrono::milliseconds userDeadline_;
};

} // namespace ghub::lighting::infrastructure
