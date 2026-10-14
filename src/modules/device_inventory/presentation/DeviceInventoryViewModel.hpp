#pragma once

#include "modules/device_inventory/application/ListDetectedDevices.hpp"

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <chrono>

namespace ghub::device_inventory {

// Expõe ao QML quais dispositivos conhecidos o sistema enumerou e por qual caminho (ADR-0006 §6, §7).
// Cada item de `devices` é um mapa com `name` e `summary`. O caso de uso informado precisa
// sobreviver ao ViewModel.
class DeviceInventoryViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    DeviceInventoryViewModel(const ListDetectedDevices& listDetectedDevices, std::chrono::milliseconds refreshInterval,
                             QObject* parent = nullptr);

    [[nodiscard]] QVariantList devices() const { return devices_; }

    Q_INVOKABLE void refresh();

signals:
    void devicesChanged();

private:
    const ListDetectedDevices* listDetectedDevices_; // não nulo
    QTimer timer_;
    QVariantList devices_;
};

} // namespace ghub::device_inventory
