#include "SystemTrayIcon.hpp"

#include "DesktopController.hpp"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QStyle>
#include <QSystemTrayIcon>

#include <utility>

namespace ghub::app {

bool SystemTrayIcon::trayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

SystemTrayIcon::SystemTrayIcon(DesktopController& controller, MenuSource menuSource, ProfileAction applyProfile)
    : controller_(&controller), menuSource_(std::move(menuSource)), applyProfile_(std::move(applyProfile))
{
    if (!controller.trayAvailable()) {
        return; // sem bandeja: nenhum ícone é criado e o app segue como hoje (INC-013 R2)
    }

    menu_ = std::make_unique<QMenu>();
    // O conteúdo (bateria dos dispositivos, perfis) muda; ele é remontado ao abrir o menu e quando o app avisa.
    QObject::connect(menu_.get(), &QMenu::aboutToShow, &controller, [this] { rebuildMenu(); });
    rebuildMenu();

    tray_ = std::make_unique<QSystemTrayIcon>();
    // O ícone do tema pode não existir se o pacote não instalou os ícones: cai no ícone padrão do estilo.
    QIcon icon = QIcon::fromTheme(QStringLiteral("ghub-linux"));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    tray_->setIcon(icon);
    tray_->setToolTip(QStringLiteral("Hubless"));
    tray_->setContextMenu(menu_.get());
    QObject::connect(tray_.get(), &QSystemTrayIcon::activated, &controller,
                     [&controller](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                             controller.showWindow();
                         }
                     });
    tray_->show();
}

void SystemTrayIcon::rebuildMenu()
{
    if (menu_ == nullptr) {
        return;
    }
    menu_->clear();
    DesktopController& controller = *controller_;
    for (const TrayMenuEntry& entry : menuSource_()) {
        using Kind = TrayMenuEntry::Kind;
        if (entry.kind == Kind::Separator) {
            menu_->addSeparator();
            continue;
        }
        QAction* action = menu_->addAction(entry.text);
        action->setEnabled(entry.enabled);
        switch (entry.kind) {
        case Kind::Profile:
            // Clicar num perfil aplica por esse clique (a mesma regra "só por clique" do botão Aplicar).
            QObject::connect(action, &QAction::triggered, &controller,
                             [this, name = entry.profile] { applyProfile_(name); });
            break;
        case Kind::Open:
            QObject::connect(action, &QAction::triggered, &controller, [&controller] { controller.showWindow(); });
            break;
        case Kind::Quit:
            // "Sair" só pede o encerramento: quem sai é QCoreApplication::quit(), pelo laço de eventos, para os
            // destrutores rodarem e o controle do G915 voltar ao firmware (INC-013 R3, ADR-0013/ADR-0014).
            QObject::connect(action, &QAction::triggered, &controller, [&controller] { controller.requestQuit(); });
            break;
        case Kind::Device:
        case Kind::NoProfiles:
        case Kind::Separator:
            break;
        }
    }
}

bool SystemTrayIcon::showMessage(const QString& title, const QString& message)
{
    if (tray_ == nullptr || !QSystemTrayIcon::supportsMessages()) {
        return false;
    }
    tray_->showMessage(title, message, tray_->icon());
    return true;
}

SystemTrayIcon::~SystemTrayIcon() = default; // unique_ptr esconde o ícone e libera o menu

} // namespace ghub::app
