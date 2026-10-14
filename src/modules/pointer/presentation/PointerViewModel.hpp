#pragma once

#include "modules/pointer/application/PointerReadRunner.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ghub::pointer {

// Expõe ao QML a leitura da configuração do G703 (ADR-0011). Somente leitura; nada é lido sem clique.
//  - `reading`: há uma leitura em andamento;
//  - `state`: "idle" | "reading" | "ready" | "notDetected" | "unavailable";
//  - `statusText`, `deviceName`, `connectionText`: textos pt-BR (vazios quando não se aplicam);
//  - `profiles`: mapas por perfil (index, name, active, enabled, reportRate, supportedReportRates,
//    stages, buttons, leds), todos com textos prontos para exibição;
//  - `warnings`: anomalias relatadas pelo dispositivo.
// O construtor não inicia leitura: só read() (ação do usuário) chama runner.start(). O runner precisa
// sobreviver ao ViewModel; onReadFinished é ligado, no composition root, ao sinal do runner assíncrono.
class PointerViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool reading READ reading NOTIFY pointerChanged)
    Q_PROPERTY(QString state READ state NOTIFY pointerChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY pointerChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY pointerChanged)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY pointerChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY pointerChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY pointerChanged)

public:
    explicit PointerViewModel(PointerReadRunner& runner, QObject* parent = nullptr);

    [[nodiscard]] bool reading() const;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString deviceName() const;
    [[nodiscard]] QString connectionText() const;
    [[nodiscard]] QVariantList profiles() const;
    [[nodiscard]] QStringList warnings() const;

    Q_INVOKABLE void read(); // única porta de entrada para uma leitura

    Q_SLOT void onReadFinished();

signals:
    void pointerChanged();

private:
    PointerReadRunner* runner_; // não nulo
};

} // namespace ghub::pointer
