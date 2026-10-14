#pragma once

#include "modules/camera/application/CameraBackend.hpp"
#include "modules/camera/application/PreviewRunner.hpp"
#include "modules/camera/domain/PreviewMode.hpp"

#include <expected>

namespace ghub::camera {

// Caso de uso: escolhe o modo de pré-visualização a partir da câmera lida e pede ao runner que inicie.
// Só a parte síncrona (ler + escolher) acontece aqui; iniciar a captura faz I/O e fica com o runner,
// fora da thread de UI. O resultado da captura chega de forma assíncrona por quem implementa o runner.
class StartCameraPreview {
public:
    // Os dois ports precisam sobreviver ao caso de uso.
    StartCameraPreview(CameraBackend& backend, PreviewRunner& runner) noexcept : backend_(&backend), runner_(&runner) {}

    // Devolve o modo pedido ao runner; erro = nada foi pedido.
    [[nodiscard]] std::expected<PreviewMode, PreviewError> execute() const;

private:
    CameraBackend* backend_; // não nulo
    PreviewRunner* runner_;  // não nulo
};

} // namespace ghub::camera
