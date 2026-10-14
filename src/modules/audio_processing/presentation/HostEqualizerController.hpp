#pragma once

#include "modules/audio_processing/application/AudioGraph.hpp"
#include "modules/audio_processing/application/HostEqualizer.hpp"

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace ghub::audio_processing {

// Aciona o EQ de saída do G733 no host (ADR-0016, INC-010 R10), fora da thread de UI. Nada é chamado no grafo
// na construção. Use sempre da thread dona; HostEqualizer e AudioGraph devem sobreviver a este objeto.
//
// Fila: uma operação por vez (HostEqualizer não é thread-safe). Durante uma operação ficam pendentes, no máximo:
//   - um aviso de perda (equalizerLost), que roda primeiro;
//   - uma ação liga/desliga (a mais recente substitui a anterior);
//   - a última curva (cada nova curva substitui a pendente; mudar a curva nunca descarta liga/desliga).
// Ordem de execução: perda, liga/desliga, curva. Sem retentativa.
//
// `gains` é a curva desejada pelo usuário e muda na hora; se a aplicação da curva falhar e não houver curva
// mais nova pendente, volta à curva do caso de uso e `status` explica. `enabled` reflete o caso de uso.
//
// Perda do EQ: o AudioGraphObserver pode ser chamado em qualquer thread; o aviso é reenfileirado para a
// thread dona. O destrutor desregistra o observador, espera a operação em andamento, descarta os pendentes e,
// se o EQ estiver ligado, o desliga de forma síncrona (restaura a saída padrão, R5).
class HostEqualizerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(QList<int> gains READ gains NOTIFY stateChanged)
    Q_PROPERTY(QStringList bandFrequencies READ bandFrequencies CONSTANT)
    Q_PROPERTY(int minimumGain READ minimumGain CONSTANT)
    Q_PROPERTY(int maximumGain READ maximumGain CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    HostEqualizerController(HostEqualizer& equalizer, AudioGraph& graph, QObject* parent = nullptr);
    ~HostEqualizerController() override;

    HostEqualizerController(const HostEqualizerController&) = delete;
    HostEqualizerController& operator=(const HostEqualizerController&) = delete;
    HostEqualizerController(HostEqualizerController&&) = delete;
    HostEqualizerController& operator=(HostEqualizerController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] QList<int> gains() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
    [[nodiscard]] QStringList bandFrequencies() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
    [[nodiscard]] int minimumGain() const { return EqualizerCurve::minimumGainDb; }
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
    [[nodiscard]] int maximumGain() const { return EqualizerCurve::maximumGainDb; }
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE void enable();
    Q_INVOKABLE void disable();
    // Banda fora de 0-9 ou ganho fora de -12..12: status de falha, nada é executado.
    Q_INVOKABLE void setBand(int band, int gainDb);
    // Curva plana (todas as bandas em 0 dB).
    Q_INVOKABLE void flat();

signals:
    void stateChanged();

private:
    enum class Op : std::uint8_t { Enable, Disable, Lost, Curve };

    struct Request {
        Op op;
        EqualizerCurve curve; // só para Curve
        QString reason;       // só para Lost
    };

    struct Outcome {
        Op op;
        std::optional<shared_kernel::DomainError> error;
        bool wasEnabled;
        bool enabled;
        std::array<int, EqualizerCurve::bandCount> appliedGains;
        QString reason;
    };

    class Observer;

    void submitCurve(const EqualizerCurve& curve);
    void submitToggle(Op op);
    void onLost(const QString& reason);
    void startNext();
    void start(const Request& request);
    void onFinished();

    HostEqualizer* equalizer_; // não nulo
    AudioGraph* graph_;        // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::unique_ptr<Observer> observer_;
    EqualizerCurve desired_ = EqualizerCurve::flat();
    std::optional<Op> pendingToggle_;
    std::optional<EqualizerCurve> pendingCurve_;
    std::optional<QString> pendingLost_;
    QString status_;
    bool enabled_ = false;
    bool running_ = false;
};

} // namespace ghub::audio_processing
