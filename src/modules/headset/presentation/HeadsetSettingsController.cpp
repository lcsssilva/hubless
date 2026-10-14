#include "modules/headset/presentation/HeadsetSettingsController.hpp"

#include <QDebug>
#include <QtConcurrent>

#include <utility>

namespace ghub::headset {

HeadsetSettingsController::HeadsetSettingsController(const ChangeHeadsetSetting& changeHeadsetSetting, QObject* parent)
    : QObject(parent), changeHeadsetSetting_(&changeHeadsetSetting)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &HeadsetSettingsController::onFinished);
}

HeadsetSettingsController::~HeadsetSettingsController()
{
    // A escrita não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    pending_.reset();
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

void HeadsetSettingsController::setSidetone(int level)
{
    const auto valid = SidetoneLevel::create(level);
    if (!valid.has_value()) {
        status_ = tr("Falha: %1").arg(QString::fromStdString(valid.error().detail));
        emit settingsChanged();
        return;
    }
    submit({.isLights = false, .level = *valid, .lights = HeadsetLights::Off});
}

void HeadsetSettingsController::setLights(bool on)
{
    // `level` é ignorado quando isLights; 0 é sempre válido.
    submit({
        .isLights = true,
        .level = SidetoneLevel::create(0).value(),
        .lights = on ? HeadsetLights::On : HeadsetLights::Off,
    });
}

void HeadsetSettingsController::submit(Request request)
{
    if (running_) {
        pending_ = request; // só o último pedido vale
        return;
    }
    start(request);
}

void HeadsetSettingsController::start(Request request)
{
    running_ = true;
    current_ = request;
    const ChangeHeadsetSetting& useCase = *changeHeadsetSetting_;
    watcher_.setFuture(QtConcurrent::run([&useCase, request] -> Outcome {
        if (request.isLights) {
            const auto done = useCase.setLights(request.lights);
            return done.has_value() ? Outcome{} : std::unexpected(done.error());
        }
        const auto done = useCase.setSidetone(request.level);
        return done.has_value() ? Outcome{} : std::unexpected(done.error());
    }));
    emit settingsChanged();
}

void HeadsetSettingsController::onFinished()
{
    const auto outcome = watcher_.result();
    const auto finished = std::exchange(current_, std::nullopt);
    if (outcome.has_value() && finished.has_value()) {
        if (finished->isLights) {
            lightsState_ = finished->lights == HeadsetLights::On ? QStringLiteral("on") : QStringLiteral("off");
            status_ = finished->lights == HeadsetLights::On ? tr("Luzes ligadas (respiração)") : tr("Luzes desligadas");
        } else {
            lastSidetone_ = finished->level.value();
            status_ = tr("Sidetone aplicado: %1").arg(lastSidetone_);
        }
    } else if (!outcome.has_value()) {
        qWarning("headset: ajuste falhou (código %d): %s", static_cast<int>(outcome.error().code),
                 outcome.error().detail.c_str());
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error().detail));
    }
    running_ = false;
    if (const auto next = std::exchange(pending_, std::nullopt); next.has_value()) {
        start(*next);
    }
    emit settingsChanged();
}

} // namespace ghub::headset
