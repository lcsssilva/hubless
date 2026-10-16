#include "modules/headset/presentation/HeadsetLightingController.hpp"

#include <QDebug>
#include <QtConcurrent>

#include <utility>

namespace ghub::headset {

namespace {

bool inByteRange(int value)
{
    return value >= 0 && value <= 255;
}

shared_kernel::Result<HeadsetLighting> build(int zone, int effect, int red, int green, int blue, int periodMs,
                                             int brightness)
{
    if (zone < 0 || zone > 1) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "zona fora de 0-1");
    }
    if (effect < 0 || effect > 3) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "efeito fora de 0-3");
    }
    if (!inByteRange(red) || !inByteRange(green) || !inByteRange(blue)) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument, "cor fora de 0-255");
    }
    return HeadsetLighting::create(
        static_cast<LightingZone>(zone), static_cast<LightingEffect>(effect),
        Rgb{static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue)},
        periodMs, brightness);
}

} // namespace

HeadsetLightingController::HeadsetLightingController(const ChangeHeadsetLighting& changeHeadsetLighting,
                                                     QObject* parent)
    : QObject(parent), changeHeadsetLighting_(&changeHeadsetLighting)
{
    connect(&watcher_, &QFutureWatcher<Outcome>::finished, this, &HeadsetLightingController::onFinished);
}

HeadsetLightingController::~HeadsetLightingController()
{
    // A escrita não é cancelável: espera terminar, pois o caso de uso deve continuar válido até lá.
    pending_.reset();
    watcher_.disconnect(this);
    if (running_) {
        watcher_.waitForFinished();
    }
}

void HeadsetLightingController::apply(int zone, int effect, int red, int green, int blue, int periodMs, int brightness)
{
    const auto lighting = build(zone, effect, red, green, blue, periodMs, brightness);
    if (!lighting.has_value()) {
        status_ = tr("Falha: %1").arg(QString::fromStdString(lighting.error().detail));
        emit lightingChanged();
        return;
    }
    if (running_) {
        pending_ = *lighting; // só o último pedido vale
        return;
    }
    start(*lighting);
}

void HeadsetLightingController::start(HeadsetLighting lighting)
{
    running_ = true;
    const ChangeHeadsetLighting& useCase = *changeHeadsetLighting_;
    watcher_.setFuture(QtConcurrent::run([&useCase, lighting] -> Outcome { return useCase.apply(lighting); }));
    emit lightingChanged();
}

void HeadsetLightingController::onFinished()
{
    const auto outcome = watcher_.result();
    if (outcome.has_value()) {
        status_ = tr("Luzes aplicadas (efeito volátil: o headset volta ao padrão ao religar)");
    } else {
        qWarning("headset: luzes falharam (código %d): %s", static_cast<int>(outcome.error().code),
                 outcome.error().detail.c_str());
        status_ = tr("Falha: %1").arg(QString::fromStdString(outcome.error().detail));
    }
    running_ = false;
    if (const auto next = std::exchange(pending_, std::nullopt); next.has_value()) {
        start(*next);
    }
    emit lightingChanged();
}

} // namespace ghub::headset
