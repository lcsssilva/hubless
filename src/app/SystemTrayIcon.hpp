#pragma once

#include "TrayMenuModel.hpp"

#include <QList>
#include <QString>

#include <functional>
#include <memory>

class QMenu;
class QSystemTrayIcon;

namespace ghub::app {

class DesktopController;

// Adaptador fino da bandeja do sistema (INC-013 R2/R3, ADR-0019 §2). É o único ponto de `src/app` que
// usa Qt Widgets: `QSystemTrayIcon` é deste módulo e o menu de contexto exige QApplication.
// Sem regra de negócio — traduz ativação do ícone e ações do menu em chamadas ao controlador. Fica
// coberto pelo teste de fumaça e pelo teste físico (a implementação concreta do ícone vem do tema da
// plataforma: StatusNotifierItem por D-Bus no Plasma).
class SystemTrayIcon {
public:
    // Há bandeja nesta sessão? Sem bandeja nada é criado e o app funciona como antes.
    [[nodiscard]] static bool trayAvailable();

    // Fonte do conteúdo do menu (INC-017 R3), consultada ao montar e ao reabrir o menu, e ação de um clique em perfil
    // (o mesmo caso de uso do botão Aplicar). Sem regra de negócio aqui: só traduz itens em ações.
    using MenuSource = std::function<QList<TrayMenuEntry>()>;
    using ProfileAction = std::function<void(const QString& profile)>;

    // `controller` precisa sobreviver ao ícone (é o contexto das conexões) e o QApplication já deve
    // existir. SystemTrayIcon não é QObject: menu e ícone têm dono próprio (unique_ptr).
    SystemTrayIcon(DesktopController& controller, MenuSource menuSource, ProfileAction applyProfile);
    ~SystemTrayIcon();

    SystemTrayIcon(const SystemTrayIcon&) = delete;
    SystemTrayIcon& operator=(const SystemTrayIcon&) = delete;
    SystemTrayIcon(SystemTrayIcon&&) = delete;
    SystemTrayIcon& operator=(SystemTrayIcon&&) = delete;

    [[nodiscard]] bool available() const noexcept { return tray_ != nullptr; }

    // Monta o menu de novo a partir da fonte (chamado quando bateria/perfis mudam).
    void rebuildMenu();

    // Notificação do desktop (INC-017 R2); false sem ícone de bandeja ou sem suporte a mensagens.
    [[nodiscard]] bool showMessage(const QString& title, const QString& message);

private:
    DesktopController* controller_; // não nulo
    MenuSource menuSource_;
    ProfileAction applyProfile_;
    std::unique_ptr<QSystemTrayIcon> tray_; // nulo quando não há bandeja
    std::unique_ptr<QMenu> menu_;
};

} // namespace ghub::app
