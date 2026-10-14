#pragma once

#include "modules/camera/domain/PreviewMode.hpp"
#include "shared_kernel/Result.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

namespace ghub::camera {

// Sessão de captura em andamento. Destruir a sessão encerra o stream e libera a câmera (RAII).
class VideoCaptureSession {
public:
    virtual ~VideoCaptureSession() = default;

    // Espera até `timeout` por um quadro e copia os bytes dele para `frame` (substitui o conteúdo).
    // true = quadro copiado; false = prazo esgotado sem quadro (`frame` fica como estava).
    // Erro = o stream acabou (câmera removida, falha do backend); a sessão não deve mais ser usada.
    [[nodiscard]] virtual shared_kernel::Result<bool> readFrame(std::chrono::milliseconds timeout,
                                                                std::vector<std::byte>& frame) = 0;

protected:
    VideoCaptureSession() = default;
    VideoCaptureSession(const VideoCaptureSession&) = default;
    VideoCaptureSession& operator=(const VideoCaptureSession&) = default;
    VideoCaptureSession(VideoCaptureSession&&) = default;
    VideoCaptureSession& operator=(VideoCaptureSession&&) = default;
};

// Port: inicia a captura de vídeo da câmera no modo pedido.
// Erro DeviceBusy = a câmera está transmitindo para outro programa.
class VideoCapture {
public:
    virtual ~VideoCapture() = default;
    [[nodiscard]] virtual shared_kernel::Result<std::unique_ptr<VideoCaptureSession>>
    start(const PreviewMode& mode) = 0;

protected:
    VideoCapture() = default;
    VideoCapture(const VideoCapture&) = default;
    VideoCapture& operator=(const VideoCapture&) = default;
    VideoCapture(VideoCapture&&) = default;
    VideoCapture& operator=(VideoCapture&&) = default;
};

} // namespace ghub::camera
