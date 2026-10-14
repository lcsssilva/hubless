#pragma once

#include "modules/camera/application/CameraControlAccess.hpp"
#include "modules/camera/infrastructure/V4l2Api.hpp"
#include "modules/device_inventory/api/DeviceInventory.hpp"

#include <filesystem>
#include <memory>

namespace ghub::camera {

// Leitura e escrita de controles da câmera via V4L2 (ADR-0010). Abre o nó de captura sem intenção de escrita
// (O_RDONLY, que o uvcvideo aceita para controles) e usa só QUERYCAP, QUERY_EXT_CTRL, QUERYMENU, G_CTRL e S_CTRL.
// Abrir e ler nunca gravam. `api` deve sobreviver a este objeto e às sessões que ele criou. Cada sessão é usada
// por uma thread só; a gravação aqui não ajusta nem repete nada (a validação é do caso de uso).
class V4l2ControlAccess final : public CameraControlAccess {
public:
    V4l2ControlAccess(device_inventory::UsbId target, V4l2Api& api,
                      std::filesystem::path sysfsRoot = "/sys/class/video4linux",
                      std::filesystem::path devRoot = "/dev");

    // BackendUnavailable: "câmera não detectada", open negado ou QUERYCAP falhou.
    [[nodiscard]] shared_kernel::Result<std::unique_ptr<CameraControlSession>> open() override;

private:
    device_inventory::UsbId target_;
    V4l2Api* api_;
    std::filesystem::path sysfsRoot_;
    std::filesystem::path devRoot_;
};

} // namespace ghub::camera
