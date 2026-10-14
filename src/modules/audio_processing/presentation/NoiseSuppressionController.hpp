#pragma once

#include "modules/audio_processing/application/HostNoiseSuppression.hpp"
#include "modules/audio_processing/application/NoiseSuppressionGraph.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>

namespace ghub::audio_processing {

// Aciona a redução de ruído do microfone do G733 no host (ADR-0018, INC-010 parte 2 R6/R9/R10), fora da thread de
// UI. Nada é chamado no grafo na construção. Use sempre da thread dona; HostNoiseSuppression e
// NoiseSuppressionGraph devem sobreviver a este objeto.
//
// Fila: uma operação por vez (HostNoiseSuppression não é thread-safe). Durante uma operação ficam pendentes, no
// máximo: um aviso de perda (roda primeiro), uma ação liga/desliga (a mais recente substitui a anterior) e o
// último limiar (mudar o limiar nunca descarta liga/desliga). Ordem: perda, liga/desliga, limiar. Sem retentativa.
//
// `threshold` é o desejado pelo usuário e muda na hora; se a aplicação falhar e não houver limiar mais novo
// pendente, volta ao do caso de uso e `status` explica. `enabled` reflete o caso de uso.
//
// O observador pode ser chamado em qualquer thread; o aviso é reenfileirado para a thread dona. O destrutor
// desregistra o observador, espera a operação em andamento, descarta os pendentes e, se ligado, desliga de forma
// síncrona (restaura a entrada padrão).
class NoiseSuppressionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(int threshold READ threshold NOTIFY stateChanged)
    Q_PROPERTY(int minimumThreshold READ minimumThreshold CONSTANT)
    Q_PROPERTY(int maximumThreshold READ maximumThreshold CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    NoiseSuppressionController(HostNoiseSuppression& useCase, NoiseSuppressionGraph& graph, QObject* parent = nullptr);
    ~NoiseSuppressionController() override;

    NoiseSuppressionController(const NoiseSuppressionController&) = delete;
    NoiseSuppressionController& operator=(const NoiseSuppressionController&) = delete;
    NoiseSuppressionController(NoiseSuppressionController&&) = delete;
    NoiseSuppressionController& operator=(NoiseSuppressionController&&) = delete;

    [[nodiscard]] bool busy() const { return running_; }
    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] int threshold() const { return desired_.percent(); }
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
    [[nodiscard]] int minimumThreshold() const { return VoiceThreshold::minimumPercent; }
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
    [[nodiscard]] int maximumThreshold() const { return VoiceThreshold::maximumPercent; }
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE void enable();
    Q_INVOKABLE void disable();
    // Fora de 0-99: status de falha, nada é executado.
    Q_INVOKABLE void setThreshold(int percent);

signals:
    void stateChanged();

private:
    enum class Op : std::uint8_t { Enable, Disable, Lost, Threshold };

    struct Request {
        Op op;
        VoiceThreshold threshold; // só para Threshold
        QString reason;           // só para Lost
    };

    struct Outcome {
        Op op;
        std::optional<shared_kernel::DomainError> error;
        bool wasEnabled;
        bool enabled;
        VoiceThreshold appliedThreshold;
        QString reason;
    };

    class Observer;

    void submitToggle(Op op);
    void onLost(const QString& reason);
    void startNext();
    void start(const Request& request);
    void onFinished();

    HostNoiseSuppression* useCase_; // não nulo
    NoiseSuppressionGraph* graph_;  // não nulo
    QFutureWatcher<Outcome> watcher_;
    std::unique_ptr<Observer> observer_;
    VoiceThreshold desired_ = VoiceThreshold::standard();
    std::optional<Op> pendingToggle_;
    std::optional<VoiceThreshold> pendingThreshold_;
    std::optional<QString> pendingLost_;
    QString status_;
    bool enabled_ = false;
    bool running_ = false;
};

} // namespace ghub::audio_processing
