#pragma once

#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <vector>

namespace ghub::headset {

// Zona de luz do G733 na 0x8070 fn3 (capturas C-01/C-03): 0 inferior, 1 superior.
enum class LightingZone : std::uint8_t { Lower = 0, Upper = 1 };

// Efeito da 0x8070 fn3: 00 desligado, 01 fixo, 02 respiração, 03 ciclo.
enum class LightingEffect : std::uint8_t { Off = 0, Fixed = 1, Breathing = 2, Cycle = 3 };

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const Rgb&, const Rgb&) = default;
};

// Objeto de valor imutável: cor/efeito de uma zona (INC-018 R1/R2). Valida as faixas e monta os bytes de parâmetro
// da 0x8070 fn3 exatamente no formato visto nas capturas:
//   desligado: <zona> 00 | fixo: <zona> 01 R G B 02 | respiração: <zona> 02 R G B <período BE16> 00 <brilho>
//   ciclo: <zona> 03 00 00 00 00 00 <período BE16> <brilho>
// A cor é enviada sem correção (a do G HUB não é conhecida).
class HeadsetLighting {
public:
    static constexpr int minPeriodMs = 1000;
    static constexpr int maxPeriodMs = 20000;
    static constexpr int periodStepMs = 100;
    static constexpr int minBrightness = 1;
    static constexpr int maxBrightness = 100;

    // Período e brilho só são validados para respiração e ciclo; fora da faixa -> InvalidArgument.
    [[nodiscard]] static shared_kernel::Result<HeadsetLighting> create(LightingZone zone, LightingEffect effect,
                                                                       Rgb color, int periodMs, int brightness);
    [[nodiscard]] static HeadsetLighting off(LightingZone zone) noexcept;
    // Ciclo de 5000 ms com brilho 100: o "ligar" do INC-009 (igual ao G HUB, C-01).
    [[nodiscard]] static HeadsetLighting defaultCycle(LightingZone zone) noexcept;

    [[nodiscard]] LightingZone zone() const noexcept { return zone_; }
    [[nodiscard]] LightingEffect effect() const noexcept { return effect_; }
    // Parâmetros da 0x8070 fn3 (começam pela zona).
    [[nodiscard]] std::vector<std::uint8_t> payload() const;

    friend bool operator==(const HeadsetLighting&, const HeadsetLighting&) = default;

private:
    HeadsetLighting(LightingZone zone, LightingEffect effect, Rgb color, int periodMs, int brightness) noexcept
        : zone_(zone), effect_(effect), color_(color), periodMs_(periodMs), brightness_(brightness)
    {
    }

    LightingZone zone_;
    LightingEffect effect_;
    Rgb color_;
    int periodMs_;
    int brightness_;
};

} // namespace ghub::headset
