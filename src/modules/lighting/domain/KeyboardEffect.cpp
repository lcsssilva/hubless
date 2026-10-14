#include "modules/lighting/domain/KeyboardEffect.hpp"

namespace ghub::lighting::domain {

KeyboardEffect KeyboardEffect::off()
{
    return KeyboardEffect{Off{}};
}

KeyboardEffect KeyboardEffect::staticColor(RgbColor color)
{
    return KeyboardEffect{Static{color}};
}

KeyboardEffect KeyboardEffect::breathing(RgbColor color, EffectPeriod period, EffectBrightness brightness)
{
    return KeyboardEffect{Breathing{.color = color, .period = period, .brightness = brightness}};
}

KeyboardEffect KeyboardEffect::cycle(EffectPeriod period, EffectBrightness brightness)
{
    return KeyboardEffect{Cycle{.period = period, .brightness = brightness}};
}

KeyboardEffect KeyboardEffect::wave(EffectPeriod period, EffectBrightness brightness)
{
    return KeyboardEffect{Wave{.period = period, .brightness = brightness}};
}

KeyboardEffect KeyboardEffect::ripple(RgbColor color, RippleSpeed speed)
{
    return KeyboardEffect{Ripple{.color = color, .speed = speed}};
}

KeyboardEffectKind KeyboardEffect::kind() const noexcept
{
    return static_cast<KeyboardEffectKind>(value_.index());
}

std::optional<RgbColor> KeyboardEffect::color() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<RgbColor> {
            if constexpr (requires { effect.color; }) {
                return effect.color;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

std::optional<EffectPeriod> KeyboardEffect::period() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<EffectPeriod> {
            if constexpr (requires { effect.period; }) {
                return effect.period;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

std::optional<EffectBrightness> KeyboardEffect::brightness() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<EffectBrightness> {
            if constexpr (requires { effect.brightness; }) {
                return effect.brightness;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

std::optional<RippleSpeed> KeyboardEffect::rippleSpeed() const
{
    return std::visit(
        [](const auto& effect) -> std::optional<RippleSpeed> {
            if constexpr (requires { effect.speed; }) {
                return effect.speed;
            } else {
                return std::nullopt;
            }
        },
        value_);
}

KeyboardEffect logoCompanion(const KeyboardEffect& keysEffect)
{
    // Os parâmetros sempre existem para o tipo (fábricas); o `if` só evita acesso desprotegido ao optional.
    const auto period = keysEffect.period();
    const auto brightness = keysEffect.brightness();
    const auto color = keysEffect.color();
    switch (keysEffect.kind()) {
    case KeyboardEffectKind::Wave:
        if (period.has_value() && brightness.has_value()) {
            return KeyboardEffect::cycle(*period, *brightness);
        }
        break;
    case KeyboardEffectKind::Ripple:
        if (color.has_value()) {
            return KeyboardEffect::staticColor(*color);
        }
        break;
    case KeyboardEffectKind::Off:
    case KeyboardEffectKind::Static:
    case KeyboardEffectKind::Breathing:
    case KeyboardEffectKind::Cycle:
        break;
    }
    return keysEffect;
}

} // namespace ghub::lighting::domain
