#include "modules/camera/domain/VideoModeSummary.hpp"

#include <algorithm>
#include <utility>

namespace ghub::camera {

namespace {

std::uint64_t areaOf(std::uint32_t width, std::uint32_t height)
{
    return std::uint64_t{width} * height;
}

// Maior área; empate de área: maior largura.
bool isLargerResolution(const VideoMode& mode, const VideoModeSummary& current)
{
    return std::pair{areaOf(mode.width(), mode.height()), mode.width()} >
           std::pair{areaOf(current.width, current.height), current.width};
}

} // namespace

std::vector<VideoModeSummary> summarizeModes(std::span<const VideoMode> modes)
{
    std::vector<VideoModeSummary> summaries;
    for (const auto& mode : modes) {
        const auto found = std::ranges::find(summaries, mode.format(), &VideoModeSummary::format);
        if (found == summaries.end()) {
            summaries.push_back({
                .format = mode.format(),
                .width = mode.width(),
                .height = mode.height(),
                .maxFrameRate = mode.maxFrameRate(),
            });
        } else if (mode.width() == found->width && mode.height() == found->height) {
            found->maxFrameRate = std::max(found->maxFrameRate, mode.maxFrameRate());
        } else if (isLargerResolution(mode, *found)) {
            *found = {
                .format = mode.format(),
                .width = mode.width(),
                .height = mode.height(),
                .maxFrameRate = mode.maxFrameRate(),
            };
        }
    }
    return summaries;
}

} // namespace ghub::camera
