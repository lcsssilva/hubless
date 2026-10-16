#include "LowBatteryNotifier.hpp"

#include "AppSettings.hpp"

#include <QVariantMap>

namespace ghub::app {

LowBatteryNotifier::LowBatteryNotifier(AppSettings& settings, ShowMessage show, LogMessage log, QObject* parent)
    : QObject(parent), settings_(&settings), show_(std::move(show)), log_(std::move(log))
{
    // Ligar a opção (ou mudar o limiar) reavalia a última leitura: bateria já baixa avisa uma vez.
    connect(settings_, &AppSettings::changed, this, &LowBatteryNotifier::evaluate);
}

void LowBatteryNotifier::onDevicesChanged(const QVariantList& devices)
{
    lastDevices_ = devices;
    evaluate();
}

void LowBatteryNotifier::evaluate()
{
    const auto threshold = battery::LowBatteryThreshold::create(settings_->lowBatteryThreshold());
    for (const QVariant& entry : lastDevices_) {
        const QVariantMap device = entry.toMap();
        const QString name = device.value(QStringLiteral("name")).toString();
        auto& alert = alerts_[name];
        alert.setEnabled(settings_->lowBatteryAlertEnabled());
        if (threshold) {
            alert.setThreshold(*threshold);
        }
        const int percent = device.value(QStringLiteral("percent"), -1).toInt();
        std::optional<battery::BatteryLevel> level;
        if (percent >= 0) {
            if (auto parsed = battery::BatteryLevel::fromPercent(percent)) {
                level = *parsed;
            }
        }
        const auto state = device.value(QStringLiteral("charging")).toBool() ? battery::ChargeState::Charging
                                                                             : battery::ChargeState::Discharging;
        if (!alert.observe(state, level)) {
            continue;
        }
        const QString message = QStringLiteral("%1: bateria em %2 %").arg(settings_->displayName(name)).arg(percent);
        if (!show_(QStringLiteral("Hubless"), message)) {
            log_(QStringLiteral("aviso de bateria fraca não exibido: sem ícone de bandeja nesta sessão (%1)")
                     .arg(message));
        }
    }
}

} // namespace ghub::app
