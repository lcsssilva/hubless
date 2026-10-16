#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace ghub::pointer {

// Modos de LED do ratbagd (doc/dbus.rst): 0 off, 1 on, 2 cycle, 3 breathing; qualquer outro é Unknown.
enum class LedMode : std::uint8_t { Off, On, Cycle, Breathing, Unknown };

[[nodiscard]] LedMode ledModeFromCode(std::uint32_t code) noexcept;

// Dados brutos de um LED, ainda não validados.
struct LedStateSpec {
    std::uint32_t index;
    LedMode mode;
    std::uint32_t rawMode; // valor bruto do ratbagd; só relevante quando mode == Unknown
    std::uint32_t red;
    std::uint32_t green;
    std::uint32_t blue;
    std::uint32_t colorDepth;
    std::uint32_t brightness;
    std::uint32_t effectDuration;
    std::vector<LedMode> supportedModes; // sem repetição; Unknown não é modo suportado
    // Bytes brutos de um formato não interpretado (setor onboard); vazio nos demais casos.
    std::vector<std::uint8_t> rawBytes = {};

    friend bool operator==(const LedStateSpec&, const LedStateSpec&) = default;
};

// Estado de um LED como o dispositivo o relata. Objeto de valor imutável; só existe válido.
class LedState {
public:
    [[nodiscard]] static shared_kernel::Result<LedState> create(LedStateSpec spec);

    [[nodiscard]] std::uint32_t index() const noexcept { return spec_.index; }
    [[nodiscard]] LedMode mode() const noexcept { return spec_.mode; }
    [[nodiscard]] std::uint32_t rawMode() const noexcept { return spec_.rawMode; }
    [[nodiscard]] std::uint32_t red() const noexcept { return spec_.red; }
    [[nodiscard]] std::uint32_t green() const noexcept { return spec_.green; }
    [[nodiscard]] std::uint32_t blue() const noexcept { return spec_.blue; }
    [[nodiscard]] std::uint32_t colorDepth() const noexcept { return spec_.colorDepth; }
    [[nodiscard]] std::uint32_t brightness() const noexcept { return spec_.brightness; }
    [[nodiscard]] std::uint32_t effectDuration() const noexcept { return spec_.effectDuration; }
    [[nodiscard]] const std::vector<LedMode>& supportedModes() const noexcept { return spec_.supportedModes; }
    [[nodiscard]] const std::vector<std::uint8_t>& rawBytes() const noexcept { return spec_.rawBytes; }

    friend bool operator==(const LedState&, const LedState&) = default;

private:
    explicit LedState(LedStateSpec spec) : spec_(std::move(spec)) {}

    LedStateSpec spec_;
};

} // namespace ghub::pointer
