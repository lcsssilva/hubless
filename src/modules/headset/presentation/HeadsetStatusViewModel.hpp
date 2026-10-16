#pragma once

#include "modules/headset/application/ReadHeadsetStatus.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ghub::headset {

// Expõe ao QML a leitura de estado do G733 por HID++ (INC-015 parte 2, R10/R17, ADR-0025). Nada é lido na
// construção: só read(), por clique. A leitura roda numa thread de trabalho (QtConcurrent), uma por vez.
//
// `status` (vazio até a primeira leitura bem-sucedida e durante uma leitura) tem textos prontos:
//  - `equalizer`: "10 bandas" ("não informado" se a feature falta);
//  - `equalizerUnexplained`: "0c 01 (não interpretado)" (vazio se não informado);
//  - `uninterpreted`: "00 00 00 00 (não interpretado)" ("não informado" se a feature falta).
// `reading`: leitura em andamento (o botão deve ficar desabilitado). `error`: mensagem do último erro (vazia
// fora de erro; lease ocupado aparece como "ocupado: ... <dono>").
// O caso de uso (e a fonte dele) precisa sobreviver ao ViewModel; o destrutor espera a leitura em andamento.
class HeadsetStatusViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool reading READ reading NOTIFY readingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit HeadsetStatusViewModel(const ReadHeadsetStatus& useCase, QObject* parent = nullptr);
    ~HeadsetStatusViewModel() override;

    HeadsetStatusViewModel(const HeadsetStatusViewModel&) = delete;
    HeadsetStatusViewModel& operator=(const HeadsetStatusViewModel&) = delete;
    HeadsetStatusViewModel(HeadsetStatusViewModel&&) = delete;
    HeadsetStatusViewModel& operator=(HeadsetStatusViewModel&&) = delete;

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
        shared_kernel::Result<HeadsetStatus> result;
    };

    void onFinished();

    const ReadHeadsetStatus* useCase_; // não nulo; não possuído
    QFutureWatcher<Outcome> watcher_;
    QVariantMap status_;
    bool reading_ = false;
    QString error_;
};

} // namespace ghub::headset
