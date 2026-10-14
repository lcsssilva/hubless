#pragma once

#include "modules/camera/domain/FrameRate.hpp"
#include "modules/camera/domain/PixelFormat.hpp"
#include "modules/camera/domain/VideoMode.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ghub::camera {

// Resumo de um formato de pixel: maior resolução (por área) e maior taxa nela.
struct VideoModeSummary {
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
    FrameRate maxFrameRate;

    friend bool operator==(const VideoModeSummary&, const VideoModeSummary&) = default;
};

// Uma entrada por formato, na ordem da primeira aparição. Empate de área: maior largura.
// Mesma resolução em mais de um modo: vale a maior taxa.
[[nodiscard]] std::vector<VideoModeSummary> summarizeModes(std::span<const VideoMode> modes);

} // namespace ghub::camera
