#pragma once

#include "modules/camera/application/CameraBackend.hpp"
#include "modules/camera/infrastructure/V4l2Api.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <filesystem>
#include <optional>

namespace ghub::camera {

// Leitura da câmera via V4L2 (ADR-0008). Somente consultas: abre o nó sem intenção de escrita e
// usa apenas os ioctls de enumeração e de leitura de controle. `api` deve sobreviver a este objeto.
class V4l2CameraBackend final : public CameraBackend {
public:
    V4l2CameraBackend(device_inventory::UsbId target, V4l2Api& api,
                      std::filesystem::path sysfsRoot = "/sys/class/video4linux",
                      std::filesystem::path devRoot = "/dev");

    [[nodiscard]] shared_kernel::Result<std::optional<CameraDescription>> readCamera() override;

private:
    device_inventory::UsbId target_;
    V4l2Api* api_;
    std::filesystem::path sysfsRoot_;
    std::filesystem::path devRoot_;
};

} // namespace ghub::camera
