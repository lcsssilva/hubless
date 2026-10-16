#pragma once

#include "shared_kernel/Result.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace ghub::pointer {

// Entrada de LED de 11 bytes do setor onboard (a partir do offset 0xD0). Só os formatos com evidência própria são
// interpretados (cópias dos setores do G703 e do G915, capturas C-05):
//   desligado: 11 bytes 0x00;
//   fixo:      01 R G B 02 00 00 00 00 00 00;
//   ciclo:     03 00 00 00 00 00 <período ms BE16> <brilho %> 00 00.
// Qualquer outra coisa é Raw: os bytes são guardados e reescritos sem alteração. Objeto de valor imutável.
class LedSetting {
public:
    static constexpr std::size_t kEntrySize = 11;
    enum class Kind : std::uint8_t { Off, Fixed, Cycle, Raw };

    [[nodiscard]] static LedSetting off() noexcept { return LedSetting{Kind::Off, {}}; }
    [[nodiscard]] static LedSetting fixed(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept;
    // Período 1..65535 ms; brilho 0..100 %.
    [[nodiscard]] static shared_kernel::Result<LedSetting> cycle(std::uint32_t periodMs, std::uint32_t brightness);
    [[nodiscard]] static LedSetting decode(std::span<const std::uint8_t, kEntrySize> entry) noexcept;

    [[nodiscard]] std::array<std::uint8_t, kEntrySize> encode() const noexcept { return bytes_; }
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    // Só significativos para o tipo correspondente (0 nos demais).
    [[nodiscard]] std::uint32_t red() const noexcept { return kind_ == Kind::Fixed ? bytes_[1] : 0U; }
    [[nodiscard]] std::uint32_t green() const noexcept { return kind_ == Kind::Fixed ? bytes_[2] : 0U; }
    [[nodiscard]] std::uint32_t blue() const noexcept { return kind_ == Kind::Fixed ? bytes_[3] : 0U; }
    [[nodiscard]] std::uint32_t periodMs() const noexcept
    {
        return kind_ == Kind::Cycle ? (static_cast<std::uint32_t>(bytes_[6]) << 8U) | bytes_[7] : 0U;
    }
    [[nodiscard]] std::uint32_t brightness() const noexcept { return kind_ == Kind::Cycle ? bytes_[8] : 0U; }

    friend bool operator==(const LedSetting&, const LedSetting&) = default;

private:
    LedSetting(Kind kind, std::array<std::uint8_t, kEntrySize> bytes) noexcept : kind_(kind), bytes_(bytes) {}

    Kind kind_;
    std::array<std::uint8_t, kEntrySize> bytes_;
};

} // namespace ghub::pointer
