#include "modules/camera/domain/PreviewMode.hpp"

namespace ghub::camera {

namespace {

constexpr std::uint32_t kMaxWidth = 1280;
constexpr std::uint32_t kMaxHeight = 720;

bool isDecodable(const PixelFormat& format)
{
    return format.fourcc() == "MJPG" || format.fourcc() == "YUYV";
}

std::uint64_t area(const VideoMode& mode)
{
    return std::uint64_t{mode.width()} * mode.height();
}

// Vence por maior área, depois maior taxa máxima, depois YUYV. Empate total: não é melhor (mantém o primeiro).
bool isBetter(const VideoMode& candidate, const VideoMode& current)
{
    if (area(candidate) != area(current)) {
        return area(candidate) > area(current);
    }
    if (candidate.maxFrameRate() != current.maxFrameRate()) {
        return candidate.maxFrameRate() > current.maxFrameRate();
    }
    return candidate.format().fourcc() == "YUYV" && current.format().fourcc() != "YUYV";
}

FrameRate minimumFrameRate()
{
    return FrameRate::create(15, 1).value();
}

} // namespace

std::optional<PreviewMode> selectPreviewMode(std::span<const VideoMode> modes)
{
    const VideoMode* best = nullptr;
    for (const auto& candidate : modes) {
        if (!isDecodable(candidate.format()) || candidate.width() > kMaxWidth || candidate.height() > kMaxHeight ||
            candidate.maxFrameRate() < minimumFrameRate()) {
            continue;
        }
        if (best == nullptr || isBetter(candidate, *best)) {
            best = &candidate;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return PreviewMode{
        .format = best->format(),
        .width = best->width(),
        .height = best->height(),
        .frameRate = best->maxFrameRate(),
    };
}

} // namespace ghub::camera
