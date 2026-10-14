#pragma once

#include "modules/lighting/domain/EffectParameters.hpp"
#include "modules/lighting/domain/RgbColor.hpp"
#include "shared_kernel/Result.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace ghub::lighting::domain {

// Zona de iluminação do headset (o G733 tem logo e faixas segundo o HeadsetControl). O índice
// ocupa um byte no protocolo; a numeração de cada zona do G733 é conferida na parte B.
class HeadsetZone {
public:
    [[nodiscard]] static shared_kernel::Result<HeadsetZone> create(std::uint32_t index);

    [[nodiscard]] std::uint8_t index() const noexcept { return index_; }

    friend bool operator==(const HeadsetZone&, const HeadsetZone&) = default;

private:
    explicit HeadsetZone(std::uint8_t index) noexcept : index_(index) {}

    std::uint8_t index_;
};

enum class HeadsetEffectKind : std::uint8_t { Off, Static, Breathing };

// Efeito de uma zona do headset. Objeto de valor imutável.
// Faixas de período (1000..20000 ms) e brilho (1..100 %) reutilizadas do teclado: PROVISÓRIO,
// conferir na parte B. Referência: HeadsetControl 4.1.0 lib/devices/logitech_g633_g933_935.hpp:91-120
// envia breathing com período 0x0FA0 = 4000 ms e brilho 0x64 = 100, dentro da faixa.
class HeadsetEffect {
public:
    [[nodiscard]] static HeadsetEffect off(HeadsetZone zone);
    [[nodiscard]] static HeadsetEffect staticColor(HeadsetZone zone, RgbColor color);
    [[nodiscard]] static HeadsetEffect breathing(HeadsetZone zone, RgbColor color, EffectPeriod period,
                                                 EffectBrightness brightness);

    [[nodiscard]] HeadsetEffectKind kind() const noexcept;
    [[nodiscard]] HeadsetZone zone() const noexcept { return zone_; }
    [[nodiscard]] std::optional<RgbColor> color() const;
    [[nodiscard]] std::optional<EffectPeriod> period() const;
    [[nodiscard]] std::optional<EffectBrightness> brightness() const;

    friend bool operator==(const HeadsetEffect&, const HeadsetEffect&) = default;

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
    using Variant = std::variant<Off, Static, Breathing>;

    HeadsetEffect(HeadsetZone zone, Variant value) : zone_(zone), value_(value) {}

    HeadsetZone zone_;
    Variant value_;
};

} // namespace ghub::lighting::domain
