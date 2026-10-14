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

namespace ghub::app {

bool SystemTrayIcon::trayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

SystemTrayIcon::SystemTrayIcon(DesktopController& controller)
{
    if (!controller.trayAvailable()) {
        return; // sem bandeja: nenhum ícone é criado e o app segue como hoje (INC-013 R2)
    }

    menu_ = std::make_unique<QMenu>();
    QAction* showAction = menu_->addAction(QStringLiteral("Mostrar janela"));
    QObject::connect(showAction, &QAction::triggered, &controller, [&controller] { controller.showWindow(); });
    QAction* quitAction = menu_->addAction(QStringLiteral("Sair"));
    // "Sair" só pede o encerramento: quem sai é QCoreApplication::quit(), pelo laço de eventos, para os
    // destrutores rodarem e o controle do G915 voltar ao firmware (INC-013 R3, ADR-0013/ADR-0014).
    QObject::connect(quitAction, &QAction::triggered, &controller, [&controller] { controller.requestQuit(); });

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

SystemTrayIcon::~SystemTrayIcon() = default; // unique_ptr esconde o ícone e libera o menu

} // namespace ghub::app
