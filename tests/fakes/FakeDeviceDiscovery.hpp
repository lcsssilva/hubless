#pragma once

#include "modules/device_inventory/application/DeviceDiscovery.hpp"

#include <utility>
#include <vector>

namespace ghub::fakes {

// Descoberta de dispositivos que devolve o resultado configurado pelo teste.
class FakeDeviceDiscovery final : public device_inventory::DeviceDiscovery {
public:
    [[nodiscard]] shared_kernel::Result<std::vector<device_inventory::UsbId>> listDevices() override { return result_; }

    void setResult(shared_kernel::Result<std::vector<device_inventory::UsbId>> result) { result_ = std::move(result); }

private:
    shared_kernel::Result<std::vector<device_inventory::UsbId>> result_;
};

} // namespace ghub::fakes
