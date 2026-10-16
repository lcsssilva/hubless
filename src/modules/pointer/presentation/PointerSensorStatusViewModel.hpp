#pragma once

#include "modules/pointer/application/ReadPointerSensorStatus.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ghub::pointer {

// Expõe ao QML a leitura de estado do sensor do G703 por HID++ (INC-015 parte 2, R10/R17, ADR-0025). Nada é lido na
// construção: só read(), por clique. A leitura roda numa thread de trabalho (QtConcurrent), uma por vez.
//
// `status` (vazio até a primeira leitura bem-sucedida e durante uma leitura) tem textos prontos:
//  - `dpiRange`: "100–25600, passo 50" (várias faixas/valores separados por "; "; "lista inválida (bytes brutos:
//  ...)");
//  - `dpi`: "7050 (padrão 800)";
//  - `reportRate`: "1000 Hz (1 ms)" (com aviso se fora da lista suportada);
//  - `supportedReportRates`: "1000, 500, 250, 125 Hz".
// Cada campo ausente no dispositivo vale "não informado".
// `reading`: leitura em andamento (o botão deve ficar desabilitado). `error`: mensagem do último erro (vazia
// fora de erro; lease ocupado aparece como "ocupado: ... <dono>").
// O caso de uso (e a fonte dele) precisa sobreviver ao ViewModel; o destrutor espera a leitura em andamento.
class PointerSensorStatusViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool reading READ reading NOTIFY readingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit PointerSensorStatusViewModel(const ReadPointerSensorStatus& useCase, QObject* parent = nullptr);
    ~PointerSensorStatusViewModel() override;

    PointerSensorStatusViewModel(const PointerSensorStatusViewModel&) = delete;
    PointerSensorStatusViewModel& operator=(const PointerSensorStatusViewModel&) = delete;
    PointerSensorStatusViewModel(PointerSensorStatusViewModel&&) = delete;
    PointerSensorStatusViewModel& operator=(PointerSensorStatusViewModel&&) = delete;

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
        shared_kernel::Result<PointerSensorStatus> result;
    };

    void onFinished();

    const ReadPointerSensorStatus* useCase_; // não nulo; não possuído
    QFutureWatcher<Outcome> watcher_;
    QVariantMap status_;
    bool reading_ = false;
    QString error_;
};

} // namespace ghub::pointer
