#include "modules/audio_processing/presentation/HostEqualizerController.hpp"

#include <QtConcurrent>

#include <utility>

namespace ghub::audio_processing {

// Chamado pelo adaptador em qualquer thread: só reenfileira o aviso para a thread dona do controlador.
class HostEqualizerController::Observer final : public AudioGraphObserver {
public:
    explicit Observer(HostEqualizerController& owner) : owner_(&owner) {}

    void equalizerLost(const std::string& reason) override
    {
        HostEqualizerController* owner = owner_;
        QMetaObject::invokeMethod(
            owner, [owner, text = QString::fromStdString(reason)] { owner->onLost(text); }, Qt::QueuedConnection);
    }

private:
    HostEqualizerController* owner_;
};

HostEqualizerController::HostEqualizerController(HostEqualizer& equalizer, AudioGraph& graph, QObject* parent)
    : QObject(parent), equalizer_(&equalizer), graph_(&graph), observer_(std::make_unique<Observer>(*this))
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &HostEqualizerController::onFinished);
    graph_->setObserver(observer_.get());
}

HostEqualizerController::~HostEqualizerController()
{
    // Primeiro deixa de receber avisos; depois espera a operação em andamento (não cancelável) e descarta os
    // pendentes. Ligado -> desliga já, de forma síncrona, para restaurar a saída padrão (R5).
    graph_->setObserver(nullptr);
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
    if (equalizer_->enabled()) {
        [[maybe_unused]] const auto disabled = equalizer_->disable(); // termina desligado mesmo com erro
    }
}

QList<int> HostEqualizerController::gains() const
{
    const auto& values = desired_.gainsDb();
    return {values.begin(), values.end()};
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
QStringList HostEqualizerController::bandFrequencies() const
{
    QStringList labels;
    for (const int hz : EqualizerCurve::bandFrequenciesHz) {
        labels.push_back(hz >= 1000 ? QStringLiteral("%1k").arg(hz / 1000) : QString::number(hz));
    }
    return labels;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): leitor de Q_PROPERTY.
QStringList HostEqualizerController::presetNames() const
{
    QStringList names;
    for (const auto& preset : EqualizerPresets::all()) {
        names.push_back(QString::fromUtf8(preset.name.data(), static_cast<qsizetype>(preset.name.size())));
    }
    return names;
}

void HostEqualizerController::enable()
{
    submitToggle(Op::Enable);
}

void HostEqualizerController::disable()
{
    submitToggle(Op::Disable);
}

void HostEqualizerController::setBand(int band, int gainDb)
{
    // Índice negativo viraria um size_t enorme; withBand rejeita qualquer banda fora de 0-9.
    const auto next = desired_.withBand(band < 0 ? EqualizerCurve::bandCount : static_cast<std::size_t>(band), gainDb);
    if (!next.has_value()) {
        status_ = tr("Falha: %1").arg(QString::fromStdString(next.error().detail));
        emit stateChanged();
        return;
    }
    submitCurve(*next);
}

void HostEqualizerController::applyPreset(int index)
{
    const auto presets = EqualizerPresets::all();
    if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
        status_ = tr("Falha: predefinição inexistente");
        emit stateChanged();
        return;
    }
    submitCurve(presets[static_cast<std::size_t>(index)].curve);
}

void HostEqualizerController::setBassTreble(int bassDb, int trebleDb)
{
    const auto inRange = [](int db) {
        return db >= EqualizerCurve::minimumGainDb && db <= EqualizerCurve::maximumGainDb;
    };
    if (!inRange(bassDb) || !inRange(trebleDb)) {
        status_ = tr("Falha: grave/agudos fora de -12..12 dB");
        emit stateChanged();
        return;
    }
    submitCurve(EqualizerPresets::fromBassTreble(bassDb, trebleDb));
    bass_ = bassDb;
    treble_ = trebleDb;
    emit stateChanged();
}

void HostEqualizerController::flat()
{
    submitCurve(EqualizerCurve::flat());
}

static_assert(EqualizerCurve::bandCount == 10, "EqualizerCurveAccess usa 10 bandas");

std::array<int, EqualizerCurve::bandCount> HostEqualizerController::currentGains() const
{
    return desired_.gainsDb();
}

shared_kernel::Result<void>
HostEqualizerController::requestGains(const std::array<int, EqualizerCurve::bandCount>& gainsDb)
{
    const auto curve = EqualizerCurve::create(gainsDb);
    if (!curve.has_value()) {
        return std::unexpected(curve.error());
    }
    submitCurve(*curve);
    return {};
}

void HostEqualizerController::submitCurve(const EqualizerCurve& curve)
{
    desired_ = curve;
    bass_ = 0;
    treble_ = 0;
    pendingCurve_ = curve;
    emit stateChanged();
    startNext();
}

void HostEqualizerController::submitToggle(Op op)
{
    pendingToggle_ = op;
    startNext();
}

void HostEqualizerController::onLost(const QString& reason)
{
    pendingLost_ = reason;
    startNext();
}

void HostEqualizerController::startNext()
{
    if (running_) {
        return;
    }
    if (const auto reason = std::exchange(pendingLost_, std::nullopt); reason.has_value()) {
        start({.op = Op::Lost, .curve = desired_, .reason = *reason});
    } else if (const auto toggle = std::exchange(pendingToggle_, std::nullopt); toggle.has_value()) {
        start({.op = *toggle, .curve = desired_, .reason = {}});
    } else if (const auto curve = std::exchange(pendingCurve_, std::nullopt); curve.has_value()) {
        start({.op = Op::Curve, .curve = *curve, .reason = {}});
    }
}

void HostEqualizerController::start(const Request& request)
{
    running_ = true;
    HostEqualizer& equalizer = *equalizer_;
    watcher_.setFuture(QtConcurrent::run([&equalizer, request] -> Outcome {
        const bool wasEnabled = equalizer.enabled();
        std::optional<shared_kernel::DomainError> error;
        shared_kernel::Result<void> done;
        switch (request.op) {
        case Op::Enable:
            done = equalizer.enable();
            break;
        case Op::Disable:
            done = equalizer.disable();
            break;
        case Op::Lost:
            equalizer.equalizerLost();
            break;
        case Op::Curve:
            done = equalizer.changeCurve(request.curve);
            break;
        }
        if (!done.has_value()) {
            error = done.error();
        }
        return {
            .op = request.op,
            .error = error,
            .wasEnabled = wasEnabled,
            .enabled = equalizer.enabled(),
            .appliedGains = equalizer.curve().gainsDb(),
            .reason = request.reason,
        };
    }));
    emit stateChanged();
}

void HostEqualizerController::onFinished()
{
    const Outcome outcome = watcher_.result();
    enabled_ = outcome.enabled;
    if (outcome.error.has_value()) {
        if (outcome.op == Op::Curve && !pendingCurve_.has_value()) {
            // Sem curva mais nova pendente: a tela volta à curva que o caso de uso mantém.
            desired_ = EqualizerCurve::create(outcome.appliedGains).value();
        }
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error->detail));
    } else if (outcome.op == Op::Enable) {
        status_ = tr("EQ ligado: saída padrão = G733 + EQ");
    } else if (outcome.op == Op::Disable) {
        status_ = tr("EQ desligado");
    } else if (outcome.op == Op::Lost) {
        if (outcome.wasEnabled) {
            status_ = tr("EQ desligado: %1").arg(outcome.reason);
        }
    } else if (outcome.op == Op::Curve) {
        status_ = outcome.enabled ? tr("Curva aplicada") : tr("Curva guardada; vale ao ligar o EQ");
    }
    running_ = false;
    startNext();
    emit stateChanged();
}

} // namespace ghub::audio_processing
