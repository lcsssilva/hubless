#pragma once

#include "modules/camera/application/CameraBackend.hpp"
#include "modules/camera/application/CameraStatus.hpp"

namespace ghub::camera {

// Caso de uso: lê a câmera pelo port e a classifica em não detectada, indisponível ou pronta.
class GetCameraStatus {
public:
    // O backend precisa sobreviver ao caso de uso.
    explicit GetCameraStatus(CameraBackend& backend) noexcept : backend_(&backend) {}

    [[nodiscard]] CameraStatus execute() const;

private:
    CameraBackend* backend_; // não nulo
};

} // namespace ghub::camera
