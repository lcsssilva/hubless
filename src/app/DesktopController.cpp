#include "DesktopController.hpp"

#include <QString>

#include <utility>

namespace ghub::app {

namespace {

// Descrição curta do estado lido/gravado, exibida junto da alternância na janela (INC-013 R8).
QString describeAutostart(bool enabled)
{
    return enabled ? QStringLiteral("Inicialização automática ligada")
                   : QStringLiteral("Inicialização automática desligada");
}

} // namespace

DesktopController::DesktopController(bool trayAvailable, bool startHiddenRequested, QString autostartDirectory,
                                     QObject* parent)
    : QObject(parent), trayAvailable_(trayAvailable), startHiddenRequested_(startHiddenRequested),
      autostart_(std::move(autostartDirectory))
{
    // Só leitura: abrir a janela não pode mexer no autostart do usuário (INC-013 R8).
    refreshAutostart();
}

void DesktopController::showWindow()
{
    emit showWindowRequested();
}

void DesktopController::requestQuit()
{
    if (quitting_) {
        return; // encerramento já pedido: não pede de novo nem reabre janela
    }
    quitting_ = true;
    emit quittingChanged();
    emit quitRequested();
}

void DesktopController::setAutostart(bool enabled)
{
    const auto result = autostart_.setEnabled(enabled);
    if (!result) {
        // O estado exibido continua sendo o real (o arquivo não mudou); o motivo vai para o status.
        status_ = QString::fromStdString(result.error().detail);
        emit statusChanged();
        return;
    }
    const bool changed = enabled != autostartEnabled_;
    autostartEnabled_ = enabled;
    status_ = describeAutostart(enabled);
    emit statusChanged();
    if (changed) {
        emit autostartChanged();
    }
}

void DesktopController::refreshAutostart()
{
    const bool enabled = autostart_.isEnabled();
    const QString status = describeAutostart(enabled);
    if (enabled != autostartEnabled_) {
        autostartEnabled_ = enabled;
        emit autostartChanged();
    }
    if (status != status_) {
        status_ = status;
        emit statusChanged();
    }
}

} // namespace ghub::app
