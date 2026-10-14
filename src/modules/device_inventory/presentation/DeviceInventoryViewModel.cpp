#include "modules/device_inventory/presentation/DeviceInventoryViewModel.hpp"

#include <QStringList>
#include <QVariantMap>

namespace ghub::device_inventory {

namespace {

QString pathText(ConnectionPath path)
{
    switch (path) {
    case ConnectionPath::LightspeedReceiver:
        return DeviceInventoryViewModel::tr("receptor Lightspeed");
    case ConnectionPath::UsbCable:
        return DeviceInventoryViewModel::tr("cabo USB");
    case ConnectionPath::UsbDongle:
        return DeviceInventoryViewModel::tr("dongle USB");
    }
    return {}; // inalcançável: o switch cobre todos os enumeradores
}

QString summaryOf(const DeviceDetection& detection)
{
    if (!detection.paths.has_value()) {
        return DeviceInventoryViewModel::tr("detecção indisponível");
    }
    if (detection.paths->empty()) {
        return DeviceInventoryViewModel::tr("não detectado");
    }
    QStringList names;
    for (const auto path : *detection.paths) {
        names.append(pathText(path));
    }
    return DeviceInventoryViewModel::tr("detectado via ") + names.join(DeviceInventoryViewModel::tr(" e "));
}

} // namespace

DeviceInventoryViewModel::DeviceInventoryViewModel(const ListDetectedDevices& listDetectedDevices,
                                                   std::chrono::milliseconds refreshInterval, QObject* parent)
    : QObject(parent), listDetectedDevices_(&listDetectedDevices)
{
    timer_.setInterval(refreshInterval);
    connect(&timer_, &QTimer::timeout, this, &DeviceInventoryViewModel::refresh);
    refresh();
    timer_.start();
}

void DeviceInventoryViewModel::refresh()
{
    QVariantList devices;
    for (const auto& detection : listDetectedDevices_->execute()) {
        devices.append(QVariantMap{
            {QStringLiteral("name"), QString::fromUtf8(modelName(detection.model))},
            {QStringLiteral("summary"), summaryOf(detection)},
        });
    }
    if (devices == devices_) {
        return;
    }
    devices_ = devices;
    emit devicesChanged();
}

} // namespace ghub::device_inventory
