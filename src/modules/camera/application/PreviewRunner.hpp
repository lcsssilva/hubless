#pragma once

#include "modules/camera/domain/PreviewMode.hpp"

#include <cstdint>
#include <string>

namespace ghub::camera {

enum class PreviewFailure : std::uint8_t {
    // Síncronas, do caso de uso.
    CameraNotDetected,
    CameraUnavailable,
    NoCompatibleMode,
    // Ao iniciar a captura (runner).
    CameraBusy,
    CaptureFailed,
    // Durante o stream (runner).
    StreamFailed,
    NoFrames,
};

struct PreviewError {
    PreviewFailure reason;
    std::string detail; // detalhe do backend, quando houver

    friend bool operator==(const PreviewError&, const PreviewError&) = default;
};

// Port: executa a pré-visualização fora da thread de UI — inicia a captura no modo pedido e entrega os quadros.
// O resultado (iniciou, falhou, terminou) chega de forma assíncrona por quem implementa o port.
class PreviewRunner {
public:
    virtual ~PreviewRunner() = default;

    // Se já houver uma pré-visualização em andamento, ela é encerrada antes.
    virtual void start(const PreviewMode& mode) = 0;
    // Idempotente; ao voltar, a câmera foi liberada.
    virtual void stop() = 0;

protected:
    PreviewRunner() = default;
    PreviewRunner(const PreviewRunner&) = default;
    PreviewRunner& operator=(const PreviewRunner&) = default;
    PreviewRunner(PreviewRunner&&) = default;
    PreviewRunner& operator=(PreviewRunner&&) = default;
};

} // namespace ghub::camera
