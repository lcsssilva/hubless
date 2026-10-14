#include "modules/audio_processing/presentation/NoiseSuppressionController.hpp"

#include <QtConcurrent>

#include <utility>

namespace ghub::audio_processing {

// Chamado pelo adaptador em qualquer thread: só reenfileira o aviso para a thread dona do controlador.
class NoiseSuppressionController::Observer final : public NoiseSuppressionObserver {
public:
    explicit Observer(NoiseSuppressionController& owner) : owner_(&owner) {}

    void noiseSuppressionLost(const std::string& reason) override
    {
        NoiseSuppressionController* owner = owner_;
        QMetaObject::invokeMethod(
            owner, [owner, text = QString::fromStdString(reason)] { owner->onLost(text); }, Qt::QueuedConnection);
    }

private:
    NoiseSuppressionController* owner_;
};

NoiseSuppressionController::NoiseSuppressionController(HostNoiseSuppression& useCase, NoiseSuppressionGraph& graph,
                                                       QObject* parent)
    : QObject(parent), useCase_(&useCase), graph_(&graph), observer_(std::make_unique<Observer>(*this)),
      desired_(useCase.threshold())
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &NoiseSuppressionController::onFinished);
    graph_->setNoiseSuppressionObserver(observer_.get());
}

NoiseSuppressionController::~NoiseSuppressionController()
{
    // Primeiro deixa de receber avisos; depois espera a operação em andamento (não cancelável) e descarta os
    // pendentes. Ligado -> desliga já, de forma síncrona, para restaurar a entrada padrão.
    graph_->setNoiseSuppressionObserver(nullptr);
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
    if (useCase_->enabled()) {
        [[maybe_unused]] const auto disabled = useCase_->disable(); // termina desligado mesmo com erro
    }
}

void NoiseSuppressionController::enable()
{
    submitToggle(Op::Enable);
}

void NoiseSuppressionController::disable()
{
    submitToggle(Op::Disable);
}

void NoiseSuppressionController::setThreshold(int percent)
{
    const auto next = VoiceThreshold::create(percent);
    if (!next.has_value()) {
        status_ = tr("Falha: %1").arg(QString::fromStdString(next.error().detail));
        emit stateChanged();
        return;
    }
    desired_ = *next;
    pendingThreshold_ = *next;
    emit stateChanged();
    startNext();
}

void NoiseSuppressionController::submitToggle(Op op)
{
    pendingToggle_ = op;
    startNext();
}

void NoiseSuppressionController::onLost(const QString& reason)
{
    pendingLost_ = reason;
    startNext();
}

void NoiseSuppressionController::startNext()
{
    if (running_) {
        return;
    }
    if (const auto reason = std::exchange(pendingLost_, std::nullopt); reason.has_value()) {
        start({.op = Op::Lost, .threshold = desired_, .reason = *reason});
    } else if (const auto toggle = std::exchange(pendingToggle_, std::nullopt); toggle.has_value()) {
        start({.op = *toggle, .threshold = desired_, .reason = {}});
    } else if (const auto level = std::exchange(pendingThreshold_, std::nullopt); level.has_value()) {
        start({.op = Op::Threshold, .threshold = *level, .reason = {}});
    }
}

void NoiseSuppressionController::start(const Request& request)
{
    running_ = true;
    HostNoiseSuppression& useCase = *useCase_;
    watcher_.setFuture(QtConcurrent::run([&useCase, request] -> Outcome {
        const bool wasEnabled = useCase.enabled();
        std::optional<shared_kernel::DomainError> error;
        shared_kernel::Result<void> done;
        switch (request.op) {
        case Op::Enable:
            done = useCase.enable();
            break;
        case Op::Disable:
            done = useCase.disable();
            break;
        case Op::Lost:
            useCase.noiseSuppressionLost();
            break;
        case Op::Threshold:
            done = useCase.changeThreshold(request.threshold);
            break;
        }
        if (!done.has_value()) {
            error = done.error();
        }
        return {
            .op = request.op,
            .error = error,
            .wasEnabled = wasEnabled,
            .enabled = useCase.enabled(),
            .appliedThreshold = useCase.threshold(),
            .reason = request.reason,
        };
    }));
    emit stateChanged();
}

void NoiseSuppressionController::onFinished()
{
    const Outcome outcome = watcher_.result();
    enabled_ = outcome.enabled;
    if (outcome.error.has_value()) {
        if (outcome.op == Op::Threshold && !pendingThreshold_.has_value()) {
            // Sem limiar mais novo pendente: a tela volta ao que o caso de uso mantém.
            desired_ = outcome.appliedThreshold;
        }
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error->detail));
    } else if (outcome.op == Op::Enable) {
        status_ = tr("Redução de ruído ligada: entrada padrão = G733 + Redução de ruído");
    } else if (outcome.op == Op::Disable) {
        status_ = tr("Redução de ruído desligada");
    } else if (outcome.op == Op::Lost) {
        if (outcome.wasEnabled) {
            status_ = tr("Redução de ruído desligada: %1").arg(outcome.reason);
        }
    } else if (outcome.op == Op::Threshold) {
        status_ = outcome.enabled ? tr("Sensibilidade aplicada") : tr("Sensibilidade guardada; vale ao ligar");
    }
    running_ = false;
    startNext();
    emit stateChanged();
}

} // namespace ghub::audio_processing
