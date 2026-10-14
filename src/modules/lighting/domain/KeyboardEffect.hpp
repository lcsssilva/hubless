#pragma once

#include "modules/lighting/domain/EffectParameters.hpp"
#include "modules/lighting/domain/RgbColor.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace ghub::lighting::domain {

enum class KeyboardEffectKind : std::uint8_t { Off, Static, Breathing, Cycle, Wave, Ripple };

// Efeito de hardware do teclado (modos Off/Static/Breathing/Cycle/Wave/Ripple do OpenRGB
// release_1.0, RGBController_LogitechG915.cpp). Objeto de valor imutável: os parâmetros já
// chegam validados pelos seus tipos, então as fábricas não falham. Um parâmetro que o efeito
// não possui devolve std::nullopt.
class KeyboardEffect {
public:
    [[nodiscard]] static KeyboardEffect off();
    [[nodiscard]] static KeyboardEffect staticColor(RgbColor color);
    [[nodiscard]] static KeyboardEffect breathing(RgbColor color, EffectPeriod period, EffectBrightness brightness);
    [[nodiscard]] static KeyboardEffect cycle(EffectPeriod period, EffectBrightness brightness);
    [[nodiscard]] static KeyboardEffect wave(EffectPeriod period, EffectBrightness brightness);
    [[nodiscard]] static KeyboardEffect ripple(RgbColor color, RippleSpeed speed);

    [[nodiscard]] KeyboardEffectKind kind() const noexcept;
    [[nodiscard]] std::optional<RgbColor> color() const;
    [[nodiscard]] std::optional<EffectPeriod> period() const;
    [[nodiscard]] std::optional<EffectBrightness> brightness() const;
    [[nodiscard]] std::optional<RippleSpeed> rippleSpeed() const;

    friend bool operator==(const KeyboardEffect&, const KeyboardEffect&) = default;

private:
    struct Off {
        friend bool operator==(const Off&, const Off&) = default;
    };
    struct Static {
        RgbColor color;
        friend bool operator==(const Static&, const Static&) = default;
    };
    struct Breathing {
        RgbColor color;
        EffectPeriod period;
        EffectBrightness brightness;
        friend bool operator==(const Breathing&, const Breathing&) = default;
    };
    struct Cycle {
        EffectPeriod period;
        EffectBrightness brightness;
        friend bool operator==(const Cycle&, const Cycle&) = default;
    };
    struct Wave {
        EffectPeriod period;
        EffectBrightness brightness;
        friend bool operator==(const Wave&, const Wave&) = default;
    };
    struct Ripple {
        RgbColor color;
        RippleSpeed speed;
        friend bool operator==(const Ripple&, const Ripple&) = default;
    };
    using Variant = std::variant<Off, Static, Breathing, Cycle, Wave, Ripple>;

    explicit KeyboardEffect(Variant value) : value_(value) {}

    Variant value_;
};

// R14: efeito do logo G que acompanha o das teclas na prévia. O logo só tem Off/Static/Cycle/Breathing:
// Wave vira Cycle (mesmo período e brilho) e Ripple vira Static (mesma cor); os demais passam iguais.
[[nodiscard]] KeyboardEffect logoCompanion(const KeyboardEffect& keysEffect);

} // namespace ghub::lighting::domain
