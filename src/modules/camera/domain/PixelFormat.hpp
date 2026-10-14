#pragma once

#include "shared_kernel/Result.hpp"

#include <array>
#include <string_view>

namespace ghub::camera {

// Formato de pixel identificado por FourCC (4 caracteres ASCII imprimíveis, sem espaço).
// Objeto de valor imutável.
class PixelFormat {
public:
    [[nodiscard]] static shared_kernel::Result<PixelFormat> create(std::string_view fourcc);

    [[nodiscard]] std::string_view fourcc() const noexcept { return {code_.data(), code_.size()}; }

    friend bool operator==(const PixelFormat&, const PixelFormat&) = default;

private:
    explicit PixelFormat(std::array<char, 4> code) : code_(code) {}

    std::array<char, 4> code_;
};

} // namespace ghub::camera
