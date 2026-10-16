#pragma once

#include "modules/lighting/application/ReadKeyboardStatus.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ghub::lighting::presentation {

// Expõe ao QML a leitura de estado do G915 por HID++ (INC-015 parte 2, R10/R17, ADR-0025). Nada é lido na construção:
// só read(), por clique. A leitura roda numa thread de trabalho (QtConcurrent), uma por vez.
//
// `status` (vazio até a primeira leitura bem-sucedida e durante uma leitura) tem textos prontos:
//  - `brightness`: "100 de 100" ("não informado" se a feature falta);
//  - `brightnessUnexplained`: "05 03 (não interpretado)" (vazio se não informado);
//  - `reportRate`: "1000 Hz (1 ms)" (com aviso se fora da lista suportada; "não informado" se falta);
//  - `supportedReportRates`: "1000, 500, 250, 125 Hz" ("não informado" se falta).
// `reading`: leitura em andamento (o botão deve ficar desabilitado). `error`: mensagem do último erro (vazia
// fora de erro; lease ocupado aparece como "ocupado: ... <dono>").
// O caso de uso (e a fonte dele) precisa sobreviver ao ViewModel; o destrutor espera a leitura em andamento.
class KeyboardStatusViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool reading READ reading NOTIFY readingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit KeyboardStatusViewModel(const application::ReadKeyboardStatus& useCase, QObject* parent = nullptr);
    ~KeyboardStatusViewModel() override;

    KeyboardStatusViewModel(const KeyboardStatusViewModel&) = delete;
    KeyboardStatusViewModel& operator=(const KeyboardStatusViewModel&) = delete;
    KeyboardStatusViewModel(KeyboardStatusViewModel&&) = delete;
    KeyboardStatusViewModel& operator=(KeyboardStatusViewModel&&) = delete;

    [[nodiscard]] QVariantMap status() const { return status_; }
    [[nodiscard]] bool reading() const { return reading_; }
    [[nodiscard]] QString error() const { return error_; }

    // Leitura em andamento é ignorada.
    Q_INVOKABLE void read();

signals:
    void statusChanged();
    void readingChanged();
    void errorChanged();
    void readFinished();

private:
    struct Outcome {
        shared_kernel::Result<KeyboardStatus> result;
    };

    void onFinished();

    const application::ReadKeyboardStatus* useCase_; // não nulo; não possuído
    QFutureWatcher<Outcome> watcher_;
    QVariantMap status_;
    bool reading_ = false;
    QString error_;
};

} // namespace ghub::lighting::presentation
