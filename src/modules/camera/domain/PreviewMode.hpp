#pragma once

#include "modules/camera/domain/FrameRate.hpp"
#include "modules/camera/domain/PixelFormat.hpp"
#include "modules/camera/domain/VideoMode.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace ghub::camera {

// Modo escolhido para a pré-visualização: um formato, uma resolução e a taxa esperada.
struct PreviewMode {
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
    FrameRate frameRate;

    friend bool operator==(const PreviewMode&, const PreviewMode&) = default;
};

// Escolhe o modo da pré-visualização entre os lidos da câmera.
// Candidatos: MJPG ou YUYV (os únicos que o produto decodifica), até 1280x720 e com taxa máxima >= 15 fps.
// Vence a maior área; empate: maior taxa máxima, depois YUYV (sem decodificação), depois o primeiro da entrada.
// A taxa do resultado é a máxima do modo vencedor. Sem candidato: vazio.
[[nodiscard]] std::optional<PreviewMode> selectPreviewMode(std::span<const VideoMode> modes);

} // namespace ghub::camera
