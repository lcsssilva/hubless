#pragma once

#include "modules/battery/application/GetBatteryStatus.hpp"

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <chrono>

namespace ghub::battery {

// Expõe ao QML o estado de bateria de cada dispositivo suportado e relê periodicamente
// (ADR-0004 §4). Cada item de `devices` é um mapa com `name`, `summary`, `percent` (-1 sem nível) e `charging`.
// O caso de uso informado precisa sobreviver ao ViewModel.
class BatteryViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    BatteryViewModel(const GetBatteryStatus& getBatteryStatus, std::chrono::milliseconds refreshInterval,
                     QObject* parent = nullptr);

    [[nodiscard]] QVariantList devices() const { return devices_; }

    Q_INVOKABLE void refresh();

signals:
    void devicesChanged();

private:
    const GetBatteryStatus* getBatteryStatus_; // não nulo
    QTimer timer_;
    QVariantList devices_;
};

} // namespace ghub::battery
