#pragma once

#include "AutostartManager.hpp"

#include <QObject>
#include <QString>

namespace ghub::app {

// Política do desktop exposta à janela (INC-013 R2–R5/R8, ADR-0019 §2). Só Qt Core: não conhece
// bandeja nem widgets; recebe a disponibilidade da bandeja como dado e diz ao QML o que fazer.
// A janela decide com `hideOnClose`/`quitting`; o adaptador de bandeja (SystemTrayIcon) chama
// `showWindow`/`requestQuit`. Nada é escrito em disco na construção: só `setAutostart` grava.
class DesktopController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool hideOnClose READ hideOnClose CONSTANT)
    Q_PROPERTY(bool startHidden READ startHidden CONSTANT)
    Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)
    Q_PROPERTY(bool autostartEnabled READ autostartEnabled NOTIFY autostartChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit DesktopController(bool trayAvailable, bool startHiddenRequested = false, QString autostartDirectory = {},
                               QObject* parent = nullptr);

    DesktopController(const DesktopController&) = delete;
    DesktopController& operator=(const DesktopController&) = delete;
    DesktopController(DesktopController&&) = delete;
    DesktopController& operator=(DesktopController&&) = delete;

    [[nodiscard]] bool trayAvailable() const noexcept { return trayAvailable_; }
    // Fechar a janela esconde (e o processo continua) apenas quando existe bandeja para reabri-la.
    [[nodiscard]] bool hideOnClose() const noexcept { return trayAvailable_; }
    // `--autostart` esconde na abertura, mas só com bandeja; sem ela a janela aparece como sempre (R9).
    [[nodiscard]] bool startHidden() const noexcept { return trayAvailable_ && startHiddenRequested_; }
    [[nodiscard]] bool quitting() const noexcept { return quitting_; }
    [[nodiscard]] bool autostartEnabled() const noexcept { return autostartEnabled_; }
    [[nodiscard]] QString status() const { return status_; }

    // Pedido de mostrar/trazer a janela (bandeja ou reabertura).
    Q_INVOKABLE void showWindow();
    // Encerra de verdade: só a primeira chamada pede o encerramento; depois disso a janela não esconde mais.
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void setAutostart(bool enabled);
    // Relê o estado do `.desktop` sem escrever (ao abrir a janela e ao voltar de outra sessão).
    Q_INVOKABLE void refreshAutostart();

signals:
    void showWindowRequested();
    void quitRequested();
    void quittingChanged();
    void autostartChanged();
    void statusChanged();

private:
    bool trayAvailable_ = false;
    bool startHiddenRequested_ = false;
    bool quitting_ = false;
    bool autostartEnabled_ = false;
    QString status_;
    AutostartManager autostart_;
};

} // namespace ghub::app
