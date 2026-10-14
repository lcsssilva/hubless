#pragma once

#include "modules/device_inventory/application/DeviceDiscovery.hpp"

#include <filesystem>
#include <utility>

namespace ghub::device_inventory {

// Adaptador de DeviceDiscovery sobre /sys/bus/hid/devices. Somente leitura e somente nomes de
// entrada: nenhum arquivo (serial, uevent, report_descriptor) é aberto e os links não são seguidos.
class SysfsHidDiscovery final : public DeviceDiscovery {
public:
    explicit SysfsHidDiscovery(std::filesystem::path root = "/sys/bus/hid/devices") : root_(std::move(root)) {}

    [[nodiscard]] shared_kernel::Result<std::vector<UsbId>> listDevices() override;

private:
    std::filesystem::path root_;
};

} // namespace ghub::device_inventory
