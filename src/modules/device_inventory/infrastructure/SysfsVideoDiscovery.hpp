#pragma once

#include "modules/device_inventory/application/DeviceDiscovery.hpp"

#include <filesystem>
#include <utility>

namespace ghub::device_inventory {

// Adaptador de DeviceDiscovery sobre /sys/class/video4linux. Somente leitura e somente
// `<entrada>/device/modalias` (no máximo 256 bytes): nenhum /dev/video* é aberto. Entradas sem
// modalias, ilegíveis ou não-USB são ignoradas; a mesma câmera (video0/video1) vira um único ID.
class SysfsVideoDiscovery final : public DeviceDiscovery {
public:
    explicit SysfsVideoDiscovery(std::filesystem::path root = "/sys/class/video4linux") : root_(std::move(root)) {}

    [[nodiscard]] shared_kernel::Result<std::vector<UsbId>> listDevices() override;

private:
    std::filesystem::path root_;
};

} // namespace ghub::device_inventory
