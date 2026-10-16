#include "modules/lighting/presentation/SyncColorController.hpp"

#include <QtConcurrent>

#include <algorithm>
#include <cstdint>

namespace ghub::lighting::presentation {

namespace {

using application::SyncDevice;
using application::SyncOutcome;
using application::SyncStatus;

QString deviceName(SyncDevice device)
{
    switch (device) {
    case SyncDevice::Keyboard:
        return QStringLiteral("G915");
    case SyncDevice::Headset:
        return QStringLiteral("G733");
    case SyncDevice::Mouse:
        return QStringLiteral("G703");
    }
    return {};
}

QString line(const SyncOutcome& outcome)
{
    QString verb;
    switch (outcome.status) {
    case SyncStatus::Applied:
        verb = QStringLiteral("aplicado");
        break;
    case SyncStatus::Skipped:
        verb = QStringLiteral("pulado");
        break;
    case SyncStatus::Failed:
        verb = QStringLiteral("falhou");
        break;
    }
    QString text = deviceName(outcome.device) + QStringLiteral(": ") + verb;
    if (!outcome.detail.empty()) {
        text += QStringLiteral(" (") + QString::fromStdString(outcome.detail) + QLatin1Char(')');
    }
    return text;
}

domain::RgbColor toRgb(const QColor& color)
{
    return {static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue())};
}

} // namespace

SyncColorController::SyncColorController(const application::SyncColor& sync, QObject* parent)
    : QObject(parent), sync_(&sync)
{
    connect(&watcher_, &QFutureWatcher<std::vector<SyncOutcome>>::finished, this, &SyncColorController::onFinished);
}

SyncColorController::~SyncColorController()
{
    watcher_.disconnect(this);
    if (watcher_.isRunning()) {
        watcher_.waitForFinished();
    }
}

void SyncColorController::apply(const QColor& color, bool keyboard, bool headset, bool mouse, bool confirmMouse)
{
    if (running_) {
        return;
    }
    if (!color.isValid()) {
        status_ = QStringLiteral("Escolha uma cor válida.");
        results_.clear();
        emit changed();
        return;
    }
    if (!keyboard && !headset && !mouse) {
        status_ = QStringLiteral("Marque ao menos um dispositivo.");
        results_.clear();
        emit changed();
        return;
    }
    const auto rgb = toRgb(color);
    // Teclado na thread de UI (dona da sessão ao vivo; o envio dela é assíncrono e não bloqueia).
    keyboardOutcomes_ = keyboard ? sync_->apply({.color = rgb, .keyboard = true, .headset = false, .mouse = false,
                                                 .confirmPersistentMouse = false})
                                 : std::vector<SyncOutcome>{};
    if (!headset && !mouse) {
        publish(keyboardOutcomes_);
        emit finished();
        return;
    }
    running_ = true;
    status_ = QStringLiteral("Sincronizando...");
    results_.clear();
    emit changed();
    const auto* sync = sync_;
    watcher_.setFuture(QtConcurrent::run([sync, rgb, headset, mouse, confirmMouse] {
        return sync->apply({.color = rgb, .keyboard = false, .headset = headset, .mouse = mouse,
                            .confirmPersistentMouse = confirmMouse});
    }));
}

void SyncColorController::onFinished()
{
    auto outcomes = keyboardOutcomes_;
    const auto rest = watcher_.result();
    outcomes.insert(outcomes.end(), rest.begin(), rest.end());
    running_ = false;
    publish(outcomes);
    emit finished();
}

void SyncColorController::publish(const std::vector<SyncOutcome>& outcomes)
{
    results_.clear();
    for (const auto& outcome : outcomes) {
        results_.push_back(line(outcome));
    }
    const auto count = [&outcomes](SyncStatus status) {
        return std::ranges::count_if(outcomes, [status](const SyncOutcome& o) { return o.status == status; });
    };
    status_ = QStringLiteral("%1 aplicado(s), %2 pulado(s), %3 com falha.")
                  .arg(count(SyncStatus::Applied))
                  .arg(count(SyncStatus::Skipped))
                  .arg(count(SyncStatus::Failed));
    emit changed();
}

} // namespace ghub::lighting::presentation
