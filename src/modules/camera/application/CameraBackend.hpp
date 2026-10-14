#pragma once

#include "modules/camera/domain/CameraDescription.hpp"
#include "shared_kernel/Result.hpp"

#include <optional>

namespace ghub::camera {

// Port: leitura da câmera (ex.: V4L2). Vazio = nó de captura ausente (câmera não detectada);
// erro = backend indisponível ou dados malformados.
class CameraBackend {
public:
    virtual ~CameraBackend() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::optional<CameraDescription>> readCamera() = 0;

protected:
    CameraBackend() = default;
    CameraBackend(const CameraBackend&) = default;
    CameraBackend& operator=(const CameraBackend&) = default;
    CameraBackend(CameraBackend&&) = default;
    CameraBackend& operator=(CameraBackend&&) = default;
};

} // namespace ghub::camera
