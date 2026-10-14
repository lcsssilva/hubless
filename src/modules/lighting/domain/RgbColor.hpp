#pragma once

#include <cstdint>

namespace ghub::lighting::domain {

// Cor RGB de 8 bits por canal. Objeto de valor imutável; todo valor de uint8_t é válido (0..255).
class RgbColor {
public:
    constexpr RgbColor(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        : red_(red), green_(green), blue_(blue)
    {
    }

    [[nodiscard]] constexpr std::uint8_t red() const noexcept { return red_; }
    [[nodiscard]] constexpr std::uint8_t green() const noexcept { return green_; }
    [[nodiscard]] constexpr std::uint8_t blue() const noexcept { return blue_; }

    friend constexpr bool operator==(const RgbColor&, const RgbColor&) = default;

private:
    std::uint8_t red_;
    std::uint8_t green_;
    std::uint8_t blue_;
};

} // namespace ghub::lighting::domain
