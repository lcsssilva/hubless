#include "modules/battery/presentation/BatteryViewModel.hpp"

#include <QVariantMap>

#include <variant>

namespace ghub::battery {

namespace {

QString nameOf(device_inventory::DeviceModel model)
{
    return QString::fromUtf8(device_inventory::modelName(model));
}

QString chargeStateText(ChargeState state)
{
    switch (state) {
    case ChargeState::Charging:
        return BatteryViewModel::tr("carregando");
    case ChargeState::Discharging:
        return BatteryViewModel::tr("descarregando");
    case ChargeState::Unknown:
        return BatteryViewModel::tr("estado desconhecido");
    case ChargeState::NotCharging:
        return BatteryViewModel::tr("sem carregar");
    case ChargeState::Full:
        return BatteryViewModel::tr("carga completa");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

// Sem nível atual (estado desconhecido ou `capacity` ausente) só o estado de carga é mostrado.
QString readingSummary(const BatteryReading& reading)
{
    auto state = chargeStateText(reading.chargeState());
    const auto level = reading.currentLevel();
    if (!level.has_value()) {
        return state;
    }
    return BatteryViewModel::tr("%1% · %2").arg(level->percent()).arg(state);
}

QString summaryOf(const DeviceBatteryStatus& status)
{
    if (const auto* reading = std::get_if<BatteryReading>(&status.state)) {
        return readingSummary(*reading);
    }
    if (std::holds_alternative<NotDetected>(status.state)) {
        return BatteryViewModel::tr("não detectado");
    }
    return BatteryViewModel::tr("leitura indisponível");
}

} // namespace

BatteryViewModel::BatteryViewModel(const GetBatteryStatus& getBatteryStatus, std::chrono::milliseconds refreshInterval,
                                   QObject* parent)
    : QObject(parent), getBatteryStatus_(&getBatteryStatus)
{
    timer_.setInterval(refreshInterval);
    connect(&timer_, &QTimer::timeout, this, &BatteryViewModel::refresh);
    refresh();
    timer_.start();
}

void BatteryViewModel::refresh()
{
    QVariantList devices;
    for (const auto& status : getBatteryStatus_->execute()) {
        devices.append(QVariantMap{
            {QStringLiteral("name"), nameOf(status.model)},
            {QStringLiteral("summary"), summaryOf(status)},
        });
    }
    if (devices == devices_) {
        return;
    }
    devices_ = devices;
    emit devicesChanged();
}

} // namespace ghub::battery
