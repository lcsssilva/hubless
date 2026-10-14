#include "modules/camera/domain/FrameRate.hpp"

#include <numeric>

namespace ghub::camera {

shared_kernel::Result<FrameRate> FrameRate::create(std::uint32_t frames, std::uint32_t seconds)
{
    if (frames == 0 || seconds == 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::InvalidArgument,
                                   "taxa de quadros exige quadros e segundos maiores que zero");
    }
    const auto divisor = std::gcd(frames, seconds);
    return FrameRate{frames / divisor, seconds / divisor};
}

std::strong_ordering operator<=>(const FrameRate& lhs, const FrameRate& rhs) noexcept
{
    // a/b <=> c/d  <=>  a*d <=> c*b (em 64 bits para não estourar).
    const std::uint64_t left = std::uint64_t{lhs.frames_} * rhs.seconds_;
    const std::uint64_t right = std::uint64_t{rhs.frames_} * lhs.seconds_;
    return left <=> right;
}

} // namespace ghub::camera
