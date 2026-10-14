#include "modules/camera/domain/VideoMode.hpp"

#include <algorithm>
#include <functional>

namespace ghub::camera {

shared_kernel::Result<VideoMode> VideoMode::create(PixelFormat format, std::uint32_t width, std::uint32_t height,
                                                   std::vector<FrameRate> rates)
{
    if (width == 0 || height == 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "modo de vídeo exige largura e altura maiores que zero");
    }
    if (rates.empty()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "modo de vídeo exige ao menos uma taxa de quadros");
    }
    std::ranges::sort(rates, std::greater<>{});
    const auto repeated = std::ranges::unique(rates);
    rates.erase(repeated.begin(), repeated.end());
    return VideoMode{format, width, height, std::move(rates)};
}

} // namespace ghub::camera
