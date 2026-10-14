#pragma once

#include "modules/camera/domain/FrameRate.hpp"
#include "modules/camera/domain/PixelFormat.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <vector>

namespace ghub::camera {

// Modo de captura: formato de pixel, resolução e taxas suportadas (ordem decrescente, sem repetição).
// Objeto de valor imutável.
class VideoMode {
public:
    [[nodiscard]] static shared_kernel::Result<VideoMode> create(PixelFormat format, std::uint32_t width,
                                                                 std::uint32_t height, std::vector<FrameRate> rates);

    [[nodiscard]] const PixelFormat& format() const noexcept { return format_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<FrameRate>& frameRates() const noexcept { return rates_; }
    [[nodiscard]] FrameRate maxFrameRate() const noexcept { return rates_.front(); }

    friend bool operator==(const VideoMode&, const VideoMode&) = default;

private:
    VideoMode(PixelFormat format, std::uint32_t width, std::uint32_t height, std::vector<FrameRate> rates)
        : format_(format), width_(width), height_(height), rates_(std::move(rates))
    {
    }

    PixelFormat format_;
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<FrameRate> rates_; // nunca vazio
};

} // namespace ghub::camera
