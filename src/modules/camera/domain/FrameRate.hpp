#pragma once

#include "shared_kernel/Result.hpp"

#include <compare>
#include <cstdint>

namespace ghub::camera {

// Taxa de quadros como fração reduzida (quadros / segundos), ambos > 0. Objeto de valor imutável.
class FrameRate {
public:
    [[nodiscard]] static shared_kernel::Result<FrameRate> create(std::uint32_t frames, std::uint32_t seconds);

    [[nodiscard]] std::uint32_t frames() const noexcept { return frames_; }
    [[nodiscard]] std::uint32_t seconds() const noexcept { return seconds_; }

    // Como a fração é sempre reduzida, a igualdade estrutural coincide com a igualdade numérica.
    friend bool operator==(const FrameRate&, const FrameRate&) = default;
    friend std::strong_ordering operator<=>(const FrameRate& lhs, const FrameRate& rhs) noexcept;

private:
    FrameRate(std::uint32_t frames, std::uint32_t seconds) : frames_(frames), seconds_(seconds) {}

    std::uint32_t frames_;
    std::uint32_t seconds_;
};

} // namespace ghub::camera
