#pragma once

#include "modules/device_inventory/domain/DeviceCatalog.hpp"
#include "shared_kernel/Result.hpp"

#include <vector>

namespace ghub::device_inventory {

// Port: descoberta somente leitura dos dispositivos (VID:PID) que o sistema enumerou por uma fonte
// (ex.: nós HID ou câmeras V4L2 no sysfs). A falha indica que a fonte inteira está indisponível.
class DeviceDiscovery {
public:
    virtual ~DeviceDiscovery() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::vector<UsbId>> listDevices() = 0;

protected:
    DeviceDiscovery() = default;
    DeviceDiscovery(const DeviceDiscovery&) = default;
    DeviceDiscovery& operator=(const DeviceDiscovery&) = default;
    DeviceDiscovery(DeviceDiscovery&&) = default;
    DeviceDiscovery& operator=(DeviceDiscovery&&) = default;
};

} // namespace ghub::device_inventory
